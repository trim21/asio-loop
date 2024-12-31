// Copyright Pan Yue 2021.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// TODO:
// 1. posix::stream_descriptor need windows version
// 2. call_* need return async.Handle
// 3. _ensure_fd_no_transport
// 4. _ensure_resolve

#include <Python.h>
#include <errno.h>
#include <optional>

#include "eventloop.hxx"
#include "fmt/base.h"
#include "nanobind/nanobind.h"

using object = nb::object;

bool _hasattr(nb::object o, const char *name) {
    return PyObject_HasAttrString(o.ptr(), name);
}

void raise_dup_error() {
    PyErr_SetString(PyExc_OSError, std::system_category().message(errno).c_str());
    throw nb::python_error();
}

void EventLoop::run_forever() {
    io.run();
}

nb::object EventLoop::create_task(nb::object coro, std::optional<nb::object> name,
                                  std::optional<nb::object> context) {
    debug_print("create_task");
    nb::dict kwargs;
    kwargs["loop"] = this;
    if (context.has_value()) {
        kwargs["context"] = context.value();
    }

    auto task = py_asyncio_Task(coro, **kwargs);
    if (name.has_value()) {
        task.attr("set_name")(name.value());
    }

    return task;
}

nb::object EventLoop::run_until_complete(nb::object future) {
    debug_print("run_until_complete");
    nb::dict kwargs;
    kwargs["loop"] = this;
    auto fut = py_ensure_future(future, **kwargs);

    auto loop = this;

    debug_print("add_done_callback");
    fut.attr("add_done_callback")(nb::cpp_function([=](nb::object fut) {
        fmt::println("hello");
        loop->io.stop();
    }));

    debug_print("run_one");
    this->io.run();

    // return future.attr("result")();

    return nb::none();
}

// void EventLoop::_sock_connect_cb(object pymod_socket, object fut, object sock, object addr) {
//     try {
//         object err =
//             sock.attr("getsockopt")(pymod_socket.attr("SOL_SOCKET"),
//             pymod_socket.attr("SO_ERROR"));
//         if (err != object(0)) {
//             // TODO: print the address
//             PyErr_SetString(PyExc_OSError, "Connect call failed {address}");
//             throw_error_already_set();
//         }
//         fut.attr("set_result")(object());
//     } catch (const error_already_set &e) {
//         if (PyErr_ExceptionMatches(PyExc_BlockingIOError) ||
//             PyErr_ExceptionMatches(PyExc_InterruptedError)) {
//             PyErr_Clear();
//             // pass
//         } else if (PyErr_ExceptionMatches(PyExc_SystemExit) ||
//                    PyErr_ExceptionMatches(PyExc_KeyboardInterrupt)) {
//             // raise
//         } else {
//             PyErr_Clear();
//             fut.attr("set_exception")(std::current_exception());
//         }
//     }
// }

// void EventLoop::_sock_accept(EventLoop &loop, object fut, object sock) {
//     int fd = extract<int>(sock.attr("fileno")());
//     object conn, address;
//     try {
//         object ret = sock.attr("accept")();
//         conn = ret[0];
//         address = ret[1];
//         conn.attr("setblocking")(object(false));
//         fut.attr("set_result")(make_tuple(conn, address));
//     } catch (const error_already_set &e) {
//         if (PyErr_ExceptionMatches(PyExc_BlockingIOError) ||
//             PyErr_ExceptionMatches(PyExc_InterruptedError)) {
//             PyErr_Clear();
//             loop._async_wait_fd(fd, bind(_sock_accept, boost::ref(loop), fut, sock),
//                                 loop._write_key(fd));
//         } else if (PyErr_ExceptionMatches(PyExc_SystemExit) ||
//                    PyErr_ExceptionMatches(PyExc_KeyboardInterrupt)) {
//             // raise
//         } else {
//             PyErr_Clear();
//             fut.attr("set_exception")(std::current_exception());
//         }
//     }
// }

void EventLoop::call_later(double delay, nb::object f) {
    auto p_timer = std::make_shared<asio::steady_timer>(
        io,
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(delay)));
    p_timer->async_wait(
        asio::bind_executor(loop, [f, p_timer](const boost::system::error_code &ec) {
            nb::gil_scoped_acquire gil{};

            f();
        }));
}

void EventLoop::call_at(double when, nb::object f) {
    using sc = std::chrono::steady_clock;
    auto p_timer = std::make_shared<asio::steady_timer>(
        io, sc::duration(static_cast<sc::time_point::rep>(when)));
    p_timer->async_wait(
        asio::bind_executor(loop, [f, p_timer](const boost::system::error_code &ec) { f(); }));
}

// // TODO: support windows socket
// object EventLoop::sock_recv(object sock, size_t nbytes) {
//     int fd = extract<int>(sock.attr("fileno")());
//     int fd_dup = dup(fd);
//     if (fd_dup == -1) {
//         raise_dup_error();
//     }
//     object py_fut = create_future();
//     _async_wait_fd(
//         fd_dup,
//         [py_fut, nbytes, fd = fd_dup] {
//             PyEval_AcquireLock();
//             std::vector<char> buffer(nbytes);
//             read(fd, buffer.data(), nbytes);
//             py_fut.attr("set_result")(
//                 object(handle<>(PyBytes_FromStringAndSize(buffer.data(), nbytes))));
//             PyEval_ReleaseLock();
//         },
//         _read_key(fd));
//     return py_fut;
// }

// // TODO: support windows socket
// object EventLoop::sock_recv_into(object sock, object buffer) {
//     int fd = extract<int>(sock.attr("fileno")());
//     int fd_dup = dup(fd);
//     if (fd_dup == -1) {
//         raise_dup_error();
//     }
//     ssize_t nbytes = len(buffer);
//     object py_fut = create_future();
//     _async_wait_fd(
//         fd_dup,
//         [py_fut, nbytes, fd = fd_dup] {
//             PyEval_AcquireLock();
//             std::vector<char> buffer(nbytes);
//             ssize_t nbytes_read = read(fd, buffer.data(), nbytes);
//             py_fut.attr("set_result")(nbytes_read);
//             PyEval_ReleaseLock();
//         },
//         _read_key(fd));
//     return py_fut;
// }

// // TODO: support windows socket
// object EventLoop::sock_sendall(object sock, object data) {
//     int fd = extract<int>(sock.attr("fileno")());
//     int fd_dup = dup(fd);
//     if (fd_dup == -1) {
//         raise_dup_error();
//     }
//     char const *py_str = extract<char const *>(data.attr("decode")());
//     ssize_t py_str_len = len(data);
//     object py_fut = create_future();
//     _async_wait_fd(
//         fd_dup,
//         [py_fut, fd, py_str, py_str_len] {
//             nb::gil_scoped_acquire gil{};

//             write(fd, py_str, py_str_len);
//             py_fut.attr("set_result")(object());
//         },
//         _write_key(fd));
//     return py_fut;
// }

// // TODO: support windows socket
// object EventLoop::sock_connect(object sock, object address) {

//     if (!_hasattr(_pymod_socket, "AF_UNIX") ||
//         sock.attr("family") != _pymod_socket.attr("AF_UNIX")) {
//         // TODO: _ensure_resolve
//     }
//     object py_fut = create_future();
//     int fd = extract<int>(sock.attr("fileno")());
//     try {
//         sock.attr("connect")(address);
//         py_fut.attr("set_result")(object());
//     } catch (const error_already_set &e) {
//         if (PyErr_ExceptionMatches(PyExc_BlockingIOError) ||
//             PyErr_ExceptionMatches(PyExc_InterruptedError)) {
//             PyErr_Clear();
//             int fd_dup = dup(fd);
//             if (fd_dup == -1) {
//                 raise_dup_error();
//             }
//             _async_wait_fd(fd_dup, bind(_sock_connect_cb, _pymod_socket, py_fut, sock, address),
//                            _write_key(fd));
//         } else if (PyErr_ExceptionMatches(PyExc_SystemExit) ||
//                    PyErr_ExceptionMatches(PyExc_KeyboardInterrupt)) {
//             // raise
//         } else {
//             PyErr_Clear();
//             py_fut.attr("set_exception")(std::current_exception());
//         }
//     }
//     return py_fut;
// }

// object EventLoop::sock_accept(object sock) {
//     object py_fut = create_future();
//     _sock_accept(*this, py_fut, sock);
//     return py_fut;
// }

// // TODO: implement this
object EventLoop::sock_sendfile(object sock, object file, int offset, int count, bool fallback) {
    PyErr_SetString(PyExc_NotImplementedError, "Not implemented!");
    throw nb::python_error();
    return object();
}

// // TODO: implement this
// object EventLoop::start_tls(object transport, object protocol, object sslcontext, bool
// server_side,
//                             object server_hostname, object ssl_handshake_timeout) {
//     PyErr_SetString(PyExc_NotImplementedError, "Not implemented!");
//     throw_error_already_set();
//     return object();
// }

extern nb::object py_socket;

// TODO: use asio resolver
nb::object EventLoop::getaddrinfo(nb::object host, int port, int family, int type, int proto,
                                  int flags) {
    debug_print("getaddrinfo start");
    object py_fut = create_future();

    asio::post(loop, [=] {
        // asio::ip::tcp::resolver resolver(that->io);
        nb::gil_scoped_acquire gil;

        try {
            nb::object res = py_socket.attr("getaddrinfo")(host, port, family, type, proto, flags);
            debug_print("getaddrinfo success");
            py_fut.attr("set_result")(res);
        } catch (const nb::python_error &e) {
            debug_print("getaddrinfo failed");
            py_fut.attr("set_exception")(nb::handle(PyErr_GetRaisedException()));
        }
    });

    debug_print("getaddrinfo return");
    return py_fut;
}

// TODO: use asio resolver
object EventLoop::getnameinfo(object sockaddr, int flags) {
    debug_print("getnameaddr start");
    object py_fut = create_future();

    asio::post(loop, [=] {
        nb::gil_scoped_acquire gil;

        try {
            nb::object res = py_socket.attr("getnameaddr")(sockaddr, flags);
            debug_print("getnameaddr success");
            py_fut.attr("set_result")(res);
        } catch (const nb::python_error &e) {
            debug_print("getnameaddr failed");
            py_fut.attr("set_exception")(nb::handle(PyErr_GetRaisedException()));
        }
    });

    debug_print("getnameaddr return");
    return py_fut;
}

// void EventLoop::default_exception_handler(object context) {
//     object message = context.attr("get")(str("message"));
//     if (message == object()) {
//         message = str("Unhandled exception in event loop");
//     }

//     object exception = context.attr("get")(str("exception"));
//     object exc_info;
//     if (exception != object()) {
//         exc_info =
//             make_tuple(exception.attr("__class__"), exception, exception.attr("__traceback__"));
//     } else {
//         exc_info = object(false);
//     }
//     if (!PyObject_IsTrue(context.attr("__contains__")(str("source_traceback")).ptr()) &&
//         _exception_handler != object() &&
//         _exception_handler.attr("_source_traceback") != object()) {
//         context["handle_traceback"] = _exception_handler.attr("_source_traceback");
//     }

//     list log_lines;
//     log_lines.append(message);
//     list context_keys(context.attr("keys"));
//     context_keys.sort();
//     for (int i = 0; i < len(context_keys); i++) {
//         std::string key = extract<std::string>(context_keys[i]);
//         if (key == "message" || key == "exception") {
//             continue;
//         }
//         str value(context[key]);
//         if (key == "source_traceback") {
//             str tb = str("").join(_pymod_traceback.attr("format_list")(value));
//             value = str("Object created at (most recent call last):\n");
//             value += tb.rstrip();
//         } else if (key == "handle_traceback") {
//             str tb = str("").join(_pymod_traceback.attr("format_list")(value));
//             value = str("Handle created at (most recent call last):\n");
//             value += tb.rstrip();
//         } else {
//             value = str(value.attr("__str__")());
//         }
//         std::ostringstream stringStream;
//         stringStream << key << ": " << value;
//         log_lines.append(str(stringStream.str()));
//     }
//     list args;
//     dict kwargs;
//     args.append(str("\n").join(log_lines));
//     kwargs["exc_info"] = exc_info;
//     _py_logger.attr("error")(tuple(args), **kwargs);
// }

// void EventLoop::call_exception_handler(object context) {
//     if (_exception_handler == object()) {
//         try {
//             default_exception_handler(context);
//         } catch (const error_already_set &e) {
//             if (PyErr_ExceptionMatches(PyExc_SystemExit) ||
//                 PyErr_ExceptionMatches(PyExc_KeyboardInterrupt)) {
//                 // raise
//             } else {
//                 PyErr_Clear();
//                 list args;
//                 dict kwargs;
//                 args.append(str("Exception in default exception handler"));
//                 kwargs["exc_info"] = true;
//                 _py_logger.attr("error")(tuple(args), **kwargs);
//             }
//         }
//     } else {
//         try {
//             _exception_handler(context);
//         } catch (const error_already_set &e) {
//             if (PyErr_ExceptionMatches(PyExc_SystemExit) ||
//                 PyErr_ExceptionMatches(PyExc_KeyboardInterrupt)) {
//                 // raise
//             } else {
//                 PyObject *ptype, *pvalue, *ptraceback;
//                 PyErr_Fetch(&ptype, &pvalue, &ptraceback);
//                 PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
//                 object type{handle<>(ptype)};
//                 object value{handle<>(pvalue)};
//                 object traceback{handle<>(ptraceback)};
//                 try {
//                     dict tmp_dict;
//                     tmp_dict["message"] = str("Unhandled error in exception handler");
//                     tmp_dict["exception"] = value;
//                     tmp_dict["context"] = context;
//                     default_exception_handler(tmp_dict);
//                 } catch (const error_already_set &e) {
//                     if (PyErr_ExceptionMatches(PyExc_SystemExit) ||
//                         PyErr_ExceptionMatches(PyExc_KeyboardInterrupt)) {
//                         // raise
//                     } else {
//                         boost::python::list args;
//                         boost::python::dict kwargs;
//                         args.append(str("Exception in default exception handler"));
//                         kwargs["exc_info"] = true;
//                         _py_logger.attr("error")(tuple(args), **kwargs);
//                     }
//                 }
//             }
//         }
//     }
// }

nb::object EventLoop::create_future() {
    nb::kwargs kwargs;
    kwargs["loop"] = this;
    return py_asyncio_Future(*nb::tuple(), **kwargs);
}

// void set_default_EventLoop(const asio::io_context::strand &strand) {
//     class_<EventLoop, boost::noncopyable>("BoostAsioEventLoop", init<asio::io_context::strand
//     &>())
//         .def("call_soon", &EventLoop::call_soon)
//         .def("call_soon_thread_safe", &EventLoop::call_soon_thread_safe)
//         .def("call_later", &EventLoop::call_later)
//         .def("call_at", &EventLoop::call_at)
//         .def("time", &EventLoop::time)
//         .def("add_reader", &EventLoop::add_reader)
//         .def("remove_reader", &EventLoop::remove_reader)
//         .def("add_writer", &EventLoop::add_writer)
//         .def("remove_writer", &EventLoop::remove_writer)
//         .def("sock_recv", &EventLoop::sock_recv)
//         .def("sock_recv_into", &EventLoop::sock_recv_into)
//         .def("sock_sendall", &EventLoop::sock_sendall)
//         .def("sock_connect", &EventLoop::sock_connect)
//         .def("sock_accept", &EventLoop::sock_accept)
//         .def("sock_sendfile", &EventLoop::sock_sendfile)
//         .def("start_tls", &EventLoop::start_tls)
//         .def("getaddrinfo", &EventLoop::getaddrinfo)
//         .def("getnameinfo", &EventLoop::getnameinfo)
//         .def("set_exception_handler", &EventLoop::set_exception_handler)
//         .def("get_exception_handler", &EventLoop::get_exception_handler)
//         .def("default_exception_handler", &EventLoop::default_exception_handler)
//         .def("call_exception_handler", &EventLoop::call_exception_handler)
//         .def("create_future", &EventLoop::create_future)
//         .def("get_debug", &EventLoop::get_debug);

//     object asyncio = import("asyncio");
//     object abstract_policy = asyncio.attr("AbstractEventLoopPolicy");

//     dict method_dict;
//     std::shared_ptr<EventLoop> p_loop = std::make_shared<EventLoop>(strand);

//     method_dict["get_EventLoop"] =
//         make_function([p_loop](object e) { return object(boost::ref(*p_loop)); },
//                       default_call_policies(), boost::mpl::vector<object, object>());

//     object class_boost_policy =
//         call<object>((PyObject *)&PyType_Type, str("BoostEventLoopPolicy"),
//                      boost::python::make_tuple(abstract_policy), method_dict);

//     object boost_policy_instance = class_boost_policy.attr("__call__")();
//     asyncio.attr("set_EventLoop_policy")(boost_policy_instance);
// }
