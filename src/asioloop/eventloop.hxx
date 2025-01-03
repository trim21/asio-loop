#include <cstdlib>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <fmt/core.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

#include <Python.h>
#include <vector>

#include "asio.hxx"

#include "common.hxx"
#include "fmt/base.h"

namespace nb = nanobind;
namespace asio = boost::asio;
using error_code = boost::system::error_code;

extern nb::object py_asyncio_mod;
extern nb::object OSError;
extern nb::object py_asyncio_futures;
extern nb::object py_asyncio_Future;
extern nb::object py_asyncio_Task;
extern nb::object py_ensure_future;
extern nb::object py_socket;
extern nb::object AddressFamily;
extern nb::object SocketKind;
extern nb::object ThreadPoolExecutor;
extern nb::object futures_wrap_future;

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
        fmt::println("handler cancel");
        token->emit(asio::cancellation_type::none);
        fmt::println("handler canceled");
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

    nb::object default_executor;

public:
    EventLoop() : loop(asio::io_context::strand{this->io}), resolver(this->io) {
        this->default_executor = nb::none();
        debug.store(false);
    }

    void set_default_executor(nb::object executor) {
        this->default_executor = executor;
    }

    nb::object run_in_executor(nb::object executor, nb::object func, nb::args args);
    nb::object call_soon_threadsafe(nb::callable callback, nb::args args, nb::object context);

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

    Handler call_at(double when, nb::object f) {
        debug_print("call_at {}", when);

        auto h = Handler(nb::none());

        using sc = std::chrono::steady_clock;
        auto p_timer = std::make_shared<asio::steady_timer>(
            io, sc::duration(static_cast<sc::time_point::rep>(when)));
        p_timer->async_wait(asio::bind_cancellation_slot(
            h.token->slot(), asio::bind_executor(loop, [=](const boost::system::error_code &ec) {
                nb::gil_scoped_acquire gil{};
                f();
            })));

        return h;
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

    Handler call_later(double delay, nb::object callback, nb::args args, nb::object context) {
        debug_print("call_later {}", delay);

        auto h = Handler(context);

        auto p_timer = std::make_shared<asio::steady_timer>(
            io,
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::duration<double>(delay)));

        p_timer->async_wait(asio::bind_cancellation_slot(
            h.token->slot(), asio::bind_executor(loop, [=](const boost::system::error_code &ec) {
                nb::gil_scoped_acquire gil{};

                callback(*args);
            })));

        return h;
    }

    void run_forever() {
        py_asyncio_mod.attr("_set_running_loop")(nb::cast(this));

        auto work_guard = asio::make_work_guard(io);
        io.run();
    }

    nb::object shutdown_default_executor(std::optional<int> timeout) {
        auto py_fut = create_future();

        py_fut.attr("set_result")(nb::none());

        return py_fut;
    }

    nb::object shutdown_asyncgens() {
        auto py_fut = create_future();

        py_fut.attr("set_result")(nb::none());

        return py_fut;
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

    nb::object getaddrinfo(std::string host, int port, int family, int type, int proto, int flags);

    // TODO: NOT IMPLEMENTED
    nb::object
    sock_sendfile(nb::object sock, nb::object file, int offset, int count, bool fallback);

    nb::object create_server(nb::object protocol_factory,
                             std::optional<std::string> host,
                             std::optional<int> port,
                             int family,
                             int flags,
                             std::optional<nb::object> sock,
                             int backlog,
                             std::optional<nb::object> ssl,
                             std::optional<nb::object> reuse_address,
                             std::optional<nb::object> reuse_port,
                             std::optional<nb::object> keep_alive,
                             std::optional<nb::object> ssl_handshake_timeout,
                             std::optional<nb::object> ssl_shutdown_timeout,
                             bool start_serving);

    nb::object create_connection(nb::object protocol_factory,
                                 std::optional<nb::object> host,
                                 std::optional<nb::object> port,
                                 std::optional<nb::object> ssl,
                                 int family,
                                 int proto,
                                 int flags,
                                 std::optional<nb::object> sock,
                                 std::optional<nb::object> local_addr,
                                 std::optional<nb::object> server_hostname,
                                 std::optional<nb::object> ssl_handshake_timeout,
                                 std::optional<nb::object> ssl_shutdown_timeout,
                                 std::optional<nb::object> happy_eyeballs_delay,
                                 std::optional<nb::object> interleave);
};
