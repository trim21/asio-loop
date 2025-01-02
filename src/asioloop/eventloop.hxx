#include <exception>
#include <future>
#include <ios>
#include <optional>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>

#include "asio.hxx"

namespace nb = nanobind;
namespace asio = boost::asio;
using error_code = boost::system::error_code;

extern nb::object OSError;
extern nb::object py_asyncio_futures;
extern nb::object py_asyncio_Future;
extern nb::object py_asyncio_Task;
extern nb::object py_ensure_future;
extern nb::object py_socket;
extern nb::object AddressFamily;
extern nb::object SocketKind;

#if WIN32
static int code_page = GetACP();
#endif

#if WIN32
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

#if WIN32
    auto b = nb::cast(OSError(ec.value(), nb::cast(to_utf8(msg))));
#else
    auto b = nb::cast(OSError(ec.value(), ec.message()));
#endif
    return b;
}

class EventLoop {
private:
    asio::io_context io;
    asio::io_context::strand loop;

    std::atomic_bool debug;

public:
    EventLoop() : loop(asio::io_context::strand{io}) {}

    bool get_debug() {
        return debug.load();
    }

    void set_debug(bool enabled) {
        this->debug.store(enabled);
    }

    void stop() {
        io.stop();
    }

    void call_soon(nb::callable callback, nb::args args, nb::kwargs kwargs) {
        debug_print("call_soon");

        asio::post(this->loop.context(), [=] { callback(*args); });
    }

    template <typename T> nb::object _wrap_co_to_py_future(std::future<T> fur) {
        nb::object py_fut = create_future();

        asio::post(io, [&]() {
            try {
                auto result = fur.get();
                fmt::println("{:?}", nb::repr(nb::cast(result)).c_str());
                py_fut.attr("set_result")(result);
            } catch (const nb::python_error &e) {
                debug_print("nb::python_error {}", e.what());
                py_fut.attr("set_exception")(e.value());
            } catch (const error_code &e) {
                debug_print("error code {}", e.message());
                py_fut.attr("set_exception")(nb::value_error(e.message().c_str()));
            } catch (const std::exception &e) {
                debug_print("std::exception {}", e.what());
                py_fut.attr("set_exception")(nb::value_error(e.what()));
            }
        });

        return py_fut;
    }

    void run_forever() {
        auto work_guard = asio::make_work_guard(io);

        io.run();
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

    // TODO: use asio resolver
    nb::object getnameinfo(nb::object sockaddr, int flags) {
        debug_print("getnameaddr start");

        auto py_fut = create_future();

        using asio::ip::tcp;

        tcp::resolver r(this->io);

        tcp::resolver::results_type result;

        std::string host = nb::cast<std::string>(sockaddr[0]);
        std::string service = std::to_string(nb::cast<int>(sockaddr[1]));

        try {
            r.async_resolve(
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

    nb::object getaddrinfo(std::string host, int port, int family, int type, int proto, int flags) {
        debug_print("getaddrinfo start");

        auto py_fut = create_future();

        using asio::ip::tcp;

        tcp::resolver r(this->io);

        tcp::resolver::results_type result;

        try {
            r.async_resolve(
                host,
                std::to_string(port),
                [=](const error_code &ec, tcp::resolver::results_type iterator) {
                    debug_print("callback {} {}", ec.value(), iterator.size());
                    try {
                        nb::gil_scoped_acquire gil{};

                        if (ec) {
                            debug_print("error {}", ec.message());
                            py_fut.attr("set_exception")(error_code_to_py_error(ec));
                            return;
                        }

                        std::vector<nb::tuple> v;
                        v.reserve(result.size());

                        for (auto it = iterator.begin(); it != iterator.end(); it++) {
                            fmt::println("{} {}", it->host_name(), it->service_name());

                            v.push_back(nb::make_tuple(
                                AddressFamily(nb::cast(it->endpoint().data()->sa_family)),
                                0,  // TODO
                                0,  // TODO
                                "", // TODO
                                nb::make_tuple(
                                    it->host_name(),
                                    it->service_name()))); // TODO: ipv6 got 2 extra field
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

        debug_print("getaddrinfo return");
        return py_fut;
    }

    void call_later(double delay, nb::object f);
    void call_at(double when, nb::object f);

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
