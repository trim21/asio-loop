#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>

namespace nb = nanobind;

#include "eventloop.hxx"

nb::object py_ensure_future;

nb::object OSError;

nb::object py_asyncio_mod;

nb::object py_asyncio_futures;
nb::object py_asyncio_Future;
nb::object py_asyncio_Task;

nb::object py_socket;
nb::object AddressFamily;
nb::object SocketKind;

NB_MODULE(__asioloop, m) {
    auto builtins = m.import_("builtins");
    OSError = builtins.attr("OSError");
    OSError.inc_ref();

    auto asyncio = m.import_("asyncio");

    py_asyncio_mod = asyncio;
    py_asyncio_mod.inc_ref();

    py_ensure_future = asyncio.attr("ensure_future");
    py_ensure_future.inc_ref();

    py_asyncio_futures = asyncio.attr("futures");
    py_asyncio_futures.inc_ref();

    py_asyncio_Task = asyncio.attr("Task");
    py_asyncio_Task.inc_ref();

    py_asyncio_Future = py_asyncio_futures.attr("Future");
    py_asyncio_Future.inc_ref();

    py_socket = m.import_("socket");
    py_socket.inc_ref();

    AddressFamily = py_socket.attr("AddressFamily");
    AddressFamily.inc_ref();

    SocketKind = py_socket.attr("AddressFamily");
    SocketKind.inc_ref();

    nb::class_<Handler>(m, "Handler").def("cancel", &Handler::cancel);

    nb::class_<EventLoop>(m, "EventLoop")
        .def(nb::init<>())
        //   .def_ro("name", &EventLoop::name)
        .def("getnameinfo", &EventLoop::getnameinfo)
        // async def getaddrinfo(self, host, port, *, family=0, type=0, proto=0, flags=0)
        .def("getaddrinfo",
             &EventLoop::getaddrinfo,
             nb::arg("host"),
             nb::arg("port"),
             nb::kw_only(),
             nb::arg("family") = 0,
             nb::arg("type") = 0,
             nb::arg("proto") = 0,
             nb::arg("flags") = 0)
        .def("create_future", &EventLoop::create_future)
        .def("create_task",
             &EventLoop::create_task,
             nb::arg("coro"),
             nb::kw_only(),
             nb::arg("name") = nb::none(),
             nb::arg("context") = nb::none())
        .def("get_debug", &EventLoop::get_debug)
        .def("stop", &EventLoop::stop)
        .def("close", &EventLoop::stop)
        .def("set_debug", &EventLoop::set_debug)
        .def("call_soon", &EventLoop::call_soon)
        .def("call_at", &EventLoop::call_at)
        .def("call_later", &EventLoop::call_later)
        .def("shutdown_asyncgens", &EventLoop::shutdown_asyncgens)
        .def("shutdown_default_executor",
             &EventLoop::shutdown_default_executor,
             nb::arg("timeout").none())
        .def("create_server",
             &EventLoop::create_server,
             nb::arg("protocol_factory"),
             nb::arg("host").none(),
             nb::arg("host").none(),
             nb::kw_only(),
             nb::arg("family") = 0,
             nb::arg("flags") = 1, //  socket.AI_PASSIVE
             nb::arg("sock").none(),
             nb::arg("backlog") = 100,
             nb::arg("ssl").none(),
             nb::arg("reuse_address").none(),
             nb::arg("reuse_port").none(),
             nb::arg("keep_alive").none(),
             nb::arg("ssl_handshake_timeout").none(),
             nb::arg("ssl_shutdown_timeout").none(),
             nb::arg("start_serving") = true)
        //    .def("create_connection",
        //         &EventLoop::create_connection,
        //         nb::arg("protocol_factory"),
        //         nb::arg("host").none(),
        //         nb::arg("host").none(),
        //         nb::kw_only(),
        //         nb::arg("ssl").none(),
        //         nb::arg("family") = 0,
        //         nb::arg("proto") = 0,
        //         nb::arg("flags") = 0,
        //         nb::arg("sock").none(),
        //         nb::arg("local_addr").none(),
        //         nb::arg("server_hostname").none(),
        //         nb::arg("ssl_handshake_timeout").none(),
        //         nb::arg("ssl_shutdown_timeout").none(),
        //         nb::arg("happy_eyeballs_delay").none(),
        //         nb::arg("interleave").none())
        .def("run_forever", &EventLoop::run_forever)
        .def("run_until_complete", &EventLoop::run_until_complete);
}
