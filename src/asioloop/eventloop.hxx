#include <optional>

#include <fmt/core.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include "asio.hxx"

namespace nb = nanobind;
namespace asio = boost::asio;

extern nb::object py_asyncio_futures;
extern nb::object py_asyncio_Future;
extern nb::object py_asyncio_Task;
extern nb::object py_ensure_future;

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

    void run_forever();
    nb::object run_until_complete(nb::object future);

    nb::object create_future();

    nb::object
    create_task(nb::object coro, std::optional<nb::object> name, std::optional<nb::object> context);

    nb::object getnameinfo(nb::object host, int flags);
    nb::object getaddrinfo(nb::object host, int port, int family, int type, int proto, int flags);

    void call_later(double delay, nb::object f);
    void call_at(double when, nb::object f);

    // TODO: NOT IMPLEMENTED
    nb::object
    sock_sendfile(nb::object sock, nb::object file, int offset, int count, bool fallback);

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
