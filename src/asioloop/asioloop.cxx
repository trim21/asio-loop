#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>


namespace nb = nanobind;

#include "eventloop.hxx"

nb::object aio_ensure_future;

NB_MODULE(__asioloop, m) {
  auto asyncio = m.import_("asyncio");
  aio_ensure_future = asyncio.attr("ensure_future");
  aio_ensure_future.inc_ref();

  nb::class_<EventLoop>(m, "EventLoop")
      .def(nb::init<const std::string_view &>())
      .def_ro("name", &EventLoop::name)
      .def("call_soon", &EventLoop::call_soon,
           nb::call_guard<nb::gil_scoped_acquire>())
      .def("run_forever", &EventLoop::run_forever,
           nb::call_guard<nb::gil_scoped_acquire>())
      .def("run_until_complete", &EventLoop::run_until_complete,
           nb::call_guard<nb::gil_scoped_acquire>())
      .def("__repr__", &EventLoop::repr);
}
