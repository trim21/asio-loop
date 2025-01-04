#include <chrono>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <string>

#include <fmt/core.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

#include <Python.h>

#include "asio.hxx"

namespace nb = nanobind;
namespace asio = boost::asio;
using error_code = boost::system::error_code;

extern nb::object py_asyncio_mod;
extern nb::object OSError;
extern nb::object RuntimeError;
extern nb::object py_asyncio_futures;
extern nb::object py_asyncio_Future;
extern nb::object py_asyncio_Task;
extern nb::object py_ensure_future;
extern nb::object py_socket;
extern nb::object AddressFamily;
extern nb::object SocketKind;
extern nb::object ThreadPoolExecutor;
extern nb::object futures_wrap_future;

#define ACQUIRE_GIL                                                                                \
    nb::gil_scoped_acquire gil {}

#define RELEASE_GIL                                                                                \
    nb::gil_scoped_release nogil {}

#if OS_WIN32
static int code_page = GetACP();
#endif

#if OS_WIN32
static std::string to_utf8(std::string s) {
    int wchars_num = MultiByteToWideChar(code_page, 0, s.c_str(), -1, NULL, 0);
    wchar_t *wstr = new wchar_t[wchars_num];

    MultiByteToWideChar(code_page, 0, s.c_str(), -1, wstr, wchars_num);

    auto b = PyUnicode_FromWideChar(wstr, wchars_num);
    delete[] wstr;

    Py_ssize_t l;

    auto utf8_s = PyUnicode_AsUTF8AndSize(b, &l);

    auto ss = std::string(utf8_s, l);

    Py_DECREF(b);

    return ss;
}
#else
// no need to do anything
static std::string to_utf8(std::string s) {
    return s;
}
#endif

static nb::object error_code_to_py_error(const error_code &ec) {
    debug_print("before cast");

    auto msg = ec.message();

#if OS_WIN32
    auto b = nb::cast(OSError(ec.value(), nb::cast(to_utf8(msg))));
#else
    auto b = nb::cast(OSError(ec.value(), ec.message()));
#endif
    return b;
}

class Handler {
    bool _cancelled = false;

public:
    std::shared_ptr<asio::cancellation_signal> token;
    nb::object context;

    explicit Handler(nb::object context) {
        this->token = std::make_shared<asio::cancellation_signal>();
    }

    nb::object get_context() {
        return this->context;
    }

    void cancel() {
        _cancelled = true;
        token->emit(asio::cancellation_type::none);
    }

    bool cancelled() {
        return this->_cancelled;
    }
};

class TimerHandler {
    bool _cancelled = false;
    std::shared_ptr<asio::steady_timer> timer;

public:
    std::shared_ptr<asio::cancellation_signal> token;
    nb::object context;

    explicit TimerHandler(nb::object context, std::shared_ptr<asio::steady_timer> timer) {
        this->token = std::make_shared<asio::cancellation_signal>();
        this->timer = timer;
    }

    nb::object get_context() {
        return this->context;
    }

    void cancel() {
        _cancelled = true;
        token->emit(asio::cancellation_type::none);
    }

    bool cancelled() {
        return this->_cancelled;
    }
};

class EventLoop {
private:
    asio::io_context io;
    asio::io_context::strand loop;

    std::atomic_bool debug;

    asio::ip::tcp::resolver resolver;

    bool closed = false;

    nb::object exception_handler = nb::none();
    nb::object default_exception_handler;

public:
    EventLoop() : loop(asio::io_context::strand{this->io}), resolver(this->io) {
        debug.store(true);
        this->default_exception_handler = nb::cpp_function(
            [=](nb::object c) { fmt::println("exception {}", nb::repr(c).c_str()); });
    }

    void call_exception_handler(nb::object context) {
        if (this->exception_handler.is_none()) {
            this->default_exception_handler(context);
            return;
        }

        this->exception_handler(context);
    }

    nb::object get_exception_handler() {
        if (this->exception_handler.is_none()) {
            return this->default_exception_handler;
        }
        return this->exception_handler;
    }

    void set_exception_handler(nb::object handler) {
        this->exception_handler = handler;
    }

    nb::object call_soon_threadsafe(nb::callable callback, nb::args args, nb::object context);

    void close() {
        this->closed = true;
    }

    bool is_closed() {
        return this->closed;
    }

    bool get_debug() {
        return debug.load();
    }

    void set_debug(bool enabled) {
        this->debug.store(enabled);
    }

    void stop() {
        io.stop();
    }

    Handler call_soon(nb::callable callback, nb::args args, nb::object context) {
        debug_print("call_soon");

        auto h = Handler(context);

        asio::dispatch(this->loop.context(), asio::bind_cancellation_slot(h.token->slot(), [=] {
                           nb::gil_scoped_acquire gil{};
                           callback(*args);
                       }));

        return h;
    }

    TimerHandler call_at(double when, nb::object f) {
        debug_print("call_at {}", when);

        using sc = std::chrono::steady_clock;
        auto p_timer = std::make_shared<asio::steady_timer>(
            io, sc::duration(static_cast<sc::time_point::rep>(when)));

        auto h = TimerHandler(nb::none(), p_timer);

        p_timer->async_wait(asio::bind_cancellation_slot(
            h.token->slot(), asio::bind_executor(loop, [=](const boost::system::error_code &ec) {
                nb::gil_scoped_acquire gil{};

                f();
            })));

        return h;
    }

    TimerHandler call_later(double delay, nb::object callback, nb::args args, nb::object context) {
        debug_print("call_later {}", delay);

        auto p_timer = std::make_shared<asio::steady_timer>(
            io,
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::duration<double>(delay)));

        auto h = TimerHandler(context, p_timer);

        p_timer->async_wait(asio::bind_cancellation_slot(
            h.token->slot(), asio::bind_executor(loop, [=](const boost::system::error_code &ec) {
                nb::gil_scoped_acquire gil{};

                callback(*args);
            })));

        return h;
    }

    bool running = false;

    bool is_running() {
        return running;
    }

    void run_forever() {
        ACQUIRE_GIL;

        py_asyncio_mod.attr("_set_running_loop")(this);

        auto work_guard = asio::make_work_guard(io);
        running = true;
        io.run();
        running = false;
    }

    nb::object run_until_complete(nb::object future) {
        debug_print("run_until_complete");
        nb::dict kwargs;
        kwargs["loop"] = this;
        auto fut = py_ensure_future(future, **kwargs);

        auto loop = this;

        debug_print("add_done_callback");
        fut.attr("add_done_callback")(nb::cpp_function([=](nb::object fut) {
            debug_print("run_until_complete end");
            loop->io.stop();
        }));

        debug_print("run start");
        run_forever();
        debug_print("run end");

        return fut.attr("result")();
    }

    nb::object create_future() {
        debug_print("create_future");
        nb::kwargs kwargs;
        kwargs["loop"] = this;
        auto b = py_asyncio_Future(**kwargs);
        debug_print("create_future return");
        return b;
    }

    nb::object create_task(nb::object coro,
                           std::optional<nb::object> name,
                           std::optional<nb::object> context) {
        debug_print("create_task");
        nb::dict kwargs;
        kwargs["loop"] = this;
        if (context.has_value()) {
            kwargs["context"] = context.value();
        }

        debug_print("call py_asyncio_Task");
        auto task = py_asyncio_Task(coro, **kwargs);
        if (name.has_value()) {
            debug_print("set name {}", nb::repr(name.value()).c_str());
            task.attr("set_name")(name.value());
        }

        debug_print("return");
        return task;
    }

    nb::object getnameinfo(nb::object sockaddr, int flags) {
        debug_print("getnameaddr start");

        auto py_fut = create_future();

        using asio::ip::tcp;

        tcp::resolver::results_type result;

        std::string host = nb::cast<std::string>(sockaddr[0]);
        std::string service = std::to_string(nb::cast<int>(sockaddr[1]));

        try {
            resolver.async_resolve(
                host, service, [=](const error_code &ec, tcp::resolver::results_type iterator) {
                    debug_print("callback {} {}", ec.value(), result.size());
                    try {
                        nb::gil_scoped_acquire gil{};

                        if (ec) {
                            debug_print("error {}", ec.message());
                            py_fut.attr("set_exception")(error_code_to_py_error(ec));
                            return;
                        }

                        debug_print("{} results", result.size());

                        std::vector<nb::tuple> v;
                        v.reserve(result.size());

                        for (auto it = iterator.begin(); it != iterator.end(); it++) {
                            fmt::println("{} {}", it->host_name(), it->service_name());
                            v.push_back(nb::make_tuple(it->host_name(), it->service_name()));
                        }

                        py_fut.attr("set_result")(nb::cast(v));
                        return;
                    } catch (const std::exception &e) {
                        debug_print("error {}", to_utf8(e.what()));
                        throw;
                    }
                    return;
                });
        } catch (error_code &e) {
            debug_print("error code {}", e.value());
            py_fut.attr("set_exception")(error_code_to_py_error(e));
        } catch (const std::exception &e) {
            debug_print("error {}", to_utf8(e.what()));
            throw;
        }

        debug_print("getnameaddr return");
        return py_fut;
    }

    void noop() {}

    double time() {
        auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> ts = now.time_since_epoch();

        return ts.count();
    }

    nb::object getaddrinfo(std::string host, int port, int family, int type, int proto, int flags);

    // TODO: NOT IMPLEMENTED

    // nb::object create_connection(nb::object protocol_factory,
    //                              std::optional<nb::object> host,
    //                              std::optional<nb::object> port,
    //                              std::optional<nb::object> ssl,
    //                              int family,
    //                              int proto,
    //                              int flags,
    //                              std::optional<nb::object> sock,
    //                              std::optional<nb::object> local_addr,
    //                              std::optional<nb::object> server_hostname,
    //                              std::optional<nb::object> ssl_handshake_timeout,
    //                              std::optional<nb::object> ssl_shutdown_timeout,
    //                              std::optional<nb::object> happy_eyeballs_delay,
    //                              std::optional<nb::object> interleave);

    asio::awaitable<asio::ip::tcp::resolver::results_type>
    _getaddrinfo(std::string host, int port, int family, int type, int proto, int flags) {
        auto resolve_flags = static_cast<asio::ip::resolver_base::flags>(flags);

        auto result = co_await resolver.async_resolve(
            host, std::to_string(port), resolve_flags, asio::use_awaitable);

        co_return result;
    }

    nb::object _wrap_co_in_py_future(std::function<asio::awaitable<nb::object>()> coro) {
        auto future = create_future();

        asio::co_spawn(
            io,
            [=]() -> asio::awaitable<void> {
                try {
                    nb::object result = co_await coro();
                    future.attr("set_result")(result);
                } catch (const error_code &ec) {
                    future.attr("set_exception")(error_code_to_py_error(ec));
                } catch (const std::exception &e) {
                    future.attr("set_exception")(
                        nb::cast(RuntimeError(nb::cast(to_utf8(e.what())))));
                }
            },
            asio::detached);

        return future;
    };
};
