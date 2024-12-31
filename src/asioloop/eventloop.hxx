#include <string>

#include "common.hpp"

#if win32
#include <SDKDDKVer.h>
#endif

#include <boost/asio.hpp>
#include <fmt/core.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

namespace nb = nanobind;
namespace asio = boost::asio;

extern nb::object aio_ensure_future;

class EventLoop {
private:
  boost::asio::io_context io;

public:
  std::string name;

  EventLoop(const std::string &s) { this->name = std::string(s); }

  std::string repr() {
    return fmt::format("<asioloop.EventLoop name={:?}>", this->name);
  }

  void call_soon(nb::callable callback, nb::args args, nb::kwargs kwargs) {
    asio::post(this->io, [=] { callback(*args); });
  }

  nb::object run_until_complete(nb::object future) {
    nb::print(future);
    nb::print(aio_ensure_future);
    nb::print(aio_ensure_future(future));

    // asio::post(this->io)
    // this->io.run_one();
    // return future.attr("result")();
    return nb::none();
  }

  void run_forever() { this->io.run(); }
};
