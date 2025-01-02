#include <optional>

#include <fmt/core.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <string_view>

#include "asio.hxx"
#include "boost/asio/awaitable.hpp"
#include "boost/asio/co_spawn.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/ip/basic_resolver.hpp"
#include "boost/asio/ip/basic_resolver_results.hpp"
#include "boost/asio/ip/resolver_base.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "fmt/base.h"

namespace nb = nanobind;
namespace asio = boost::asio;
using error_code = boost::system::error_code;

extern nb::object py_asyncio_futures;
extern nb::object py_asyncio_Future;
extern nb::object py_asyncio_Task;
extern nb::object py_ensure_future;
extern nb::object py_socket;

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

        asio::post(this->io, [=] { callback(*args); });
    }

    template <typename T> nb::object _co_to_py_future(asio::awaitable<T> co) {
        return nb::none();
    }

    void run_forever();
    nb::object run_until_complete(nb::object future);

    nb::object create_future();

    nb::object
    create_task(nb::object coro, std::optional<nb::object> name, std::optional<nb::object> context);

    asio::awaitable<void> _getnameinfo(const std::string_view &host,
                                       const std::string_view &service) {
        debug_print("getnameinfo start");
        // nb::object py_fut = create_future();

        using asio::ip::tcp;

        tcp::resolver::results_type result;
        tcp::resolver r(this->io);
        try {
            result = co_await r.async_resolve(host, service, asio::use_awaitable);
        } catch (error_code &ec) {
            fmt::println("{}", ec.to_string());
            co_return;
        }

        int a = 0;
        for (auto it : result) {
            fmt::println("{} {}", a, it.host_name());
            a++;
        }
    }

    // TODO: use asio resolver
    nb::object getnameinfo(nb::object host, int flags) {
        debug_print("getnameaddr start");
        nb::print(nb::repr(host));
        debug_print("before co_spawn");
        asio::co_spawn(io, _getnameinfo(nb::cast<nb::str>(host[0]).c_str(), ""), asio::detached);
        debug_print("after co_spawn");

        nb::object py_fut = create_future();

        asio::post(loop, [=] {
            nb::gil_scoped_acquire gil;

            try {
                nb::object res = py_socket.attr("getnameinfo")(host, flags);
                debug_print("getnameaddr success");
                py_fut.attr("set_result")(res);
            } catch (const nb::python_error &e) {
                py_fut.attr("set_exception")(e.value());
            }
        });

        debug_print("getnameaddr return");

        return py_fut;
    }

    nb::object getaddrinfo(nb::object host, int port, int family, int type, int proto, int flags);

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
