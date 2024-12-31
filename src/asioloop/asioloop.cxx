#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>

namespace nb = nanobind;

#include "eventloop.hxx"

nb::object py_ensure_future;

nb::object py_asyncio_futures;
nb::object py_asyncio_Future;
nb::object py_asyncio_Task;

nb::object py_socket;

NB_MODULE(__asioloop, m) {
    auto asyncio = m.import_("asyncio");

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

    nb::class_<EventLoop>(m, "EventLoop")
        .def(nb::init<const std::string_view &>())
        .def_ro("name", &EventLoop::name)
        .def("getnameinfo", &EventLoop::getnameinfo)
        // async def getaddrinfo(self, host, port, *, family=0, type=0, proto=0, flags=0)
        .def("getaddrinfo", &EventLoop::getaddrinfo, nb::arg("host"), nb::arg("port"),
             nb::kw_only(), nb::arg("family") = 0, nb::arg("type") = 0, nb::arg("proto") = 0,
             nb::arg("flags") = 0)
        .def("create_future", &EventLoop::create_future)
        .def("create_task", &EventLoop::create_task, nb::arg("coro"), nb::kw_only(),
             nb::arg("name") = nb::none(), nb::arg("context") = nb::none())
        .def("get_debug", &EventLoop::get_debug)
        .def("call_soon", &EventLoop::call_soon)
        .def("run_forever", &EventLoop::run_forever)
        .def("run_until_complete", &EventLoop::run_until_complete)
        .def("__repr__", &EventLoop::repr);
}
