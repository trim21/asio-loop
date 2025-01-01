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

public:
    EventLoop() : loop(asio::io_context::strand{io}) {}

    // std::string repr() {
    // return fmt::format("<asioloop.EventLoop at 0x{:x}>", reinterpret_cast<size_t>(this));
    // }

    nb::object get_debug() {
        return nb::cast(true);
    }

    void call_soon(nb::callable callback, nb::args args, nb::kwargs kwargs) {
        asio::post(this->io, [=] { callback(*args); });
    }

    nb::object run_until_complete(nb::object future);

    nb::object create_future();

    nb::object create_task(nb::object coro, std::optional<nb::object> name,
                           std::optional<nb::object> context);

    nb::object getnameinfo(nb::object host, int flags);
    nb::object getaddrinfo(nb::object host, int port, int family, int type, int proto, int flags);

    void call_later(double delay, nb::object f);
    void call_at(double when, nb::object f);

    // TODO: NOT IMPLEMENTED
    nb::object sock_sendfile(nb::object sock, nb::object file, int offset, int count,
                             bool fallback);

    void run_forever();
};
