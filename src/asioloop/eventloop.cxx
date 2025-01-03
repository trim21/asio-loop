// Copyright Pan Yue 2021.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <errno.h>

#include <Python.h>
#include <fmt/base.h>
#include <nanobind/nanobind.h>

#include "eventloop.hxx"

using object = nb::object;

#define THROW_NOT_IMPLEMENT                                                                        \
    do {                                                                                           \
        PyErr_SetString(PyExc_NotImplementedError, "Not implemented!");                            \
        throw nb::python_error();                                                                  \
    } while (1)

bool _hasattr(nb::object o, const char *name) {
    return PyObject_HasAttrString(o.ptr(), name);
}

void raise_dup_error() {
    PyErr_SetString(PyExc_OSError, std::system_category().message(errno).c_str());
    throw nb::python_error();
}

// void EventLoop::run_forever()

// nb::object EventLoop::create_task(nb::object coro,
//                                   std::optional<nb::object> name,
//                                   std::optional<nb::object> context)

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

// nb::object EventLoop::run_in_executor(nb::object executor, nb::object func, nb::args args) {
//     fmt::println("run_in_executor {} {} {}",
//                  nb::repr(executor).c_str(),
//                  nb::repr(func).c_str(),
//                  nb::repr(args).c_str());

//     if (executor.is_none()) {
//         if (this->default_executor.is_none()) {
//             nb::dict kwargs;
//             kwargs["thread_name_prefix"] = "asyncio";

//             this->default_executor = ThreadPoolExecutor(**kwargs);
//         }

//         executor = this->default_executor;
//     }

//     auto t = executor.attr("submit")(func, *args);

//     t.attr("add_done_callback")(nb::cpp_function([=](nb::object f) { fmt::println("work done");
//     }));

//     auto f = futures_wrap_future(t);

//     return f;
// }

nb::object
EventLoop::call_soon_threadsafe(nb::callable callback, nb::args args, nb::object context) {
    fmt::println("call_soon_threadsafe start");

    auto h = Handler(context);

    asio::post(this->io, asio::bind_executor(this->loop, [=] {
                   fmt::println("call_soon_threadsafe run callback start");
                   RAII_GIL;
                   if (context.is_none()) {
                       callback(*args);
                   } else {
                       context.attr("run")(callback, *args);
                   }
                   fmt::println("call_soon_threadsafe run callback done");
               }));

    fmt::println("call_soon_threadsafe done");
    return nb::cast(h);
}
template <nb::rv_policy policy = nb::rv_policy::automatic, typename... Args>
nb::args make_args(Args &&...args) {
    nb::args result = nb::steal<nb::args>(PyTuple_New((Py_ssize_t)sizeof...(Args)));

    size_t nargs = 0;
    PyObject *o = result.ptr();

    (NB_TUPLE_SET_ITEM(
         o,
         nargs++,
         nb::detail::make_caster<Args>::from_cpp((nb::detail::forward_t<Args>)args, policy, nullptr)
             .ptr()),
     ...);

    nb::detail::tuple_check(o, sizeof...(Args));

    return result;
}

// nb::object
// EventLoop::getaddrinfo(std::string host, int port, int family, int type, int proto, int flags) {
//     debug_print("getaddrinfo start");
//     nb::args args;

//     return run_in_executor(nb::none(),
//                            py_socket.attr("getaddrinfo"),
//                            make_args(host, port, family, type, proto, flags));

// auto py_fut = create_future();

// // struct addrinfo s;

// using asio::ip::tcp;

// tcp::resolver::results_type result;

// try {
//     resolver.async_resolve(
//         host,
//         std::to_string(port),
//         [=](const error_code &ec, tcp::resolver::results_type iterator) {
//             debug_print("callback {} {}", ec.value(), iterator.size());
//             try {
//                 nb::gil_scoped_acquire gil{};

//                 if (ec) {
//                     debug_print("error {}", ec.message());
//                     py_fut.attr("set_exception")(error_code_to_py_error(ec));
//                     return;
//                 }

//                 std::vector<nb::tuple> v;
//                 v.reserve(result.size());

//                 for (auto it = iterator.begin(); it != iterator.end(); it++) {
//                     v.push_back(nb::make_tuple(
//                         AddressFamily(nb::cast(it->endpoint().protocol().family())),
//                         SocketKind(it->endpoint().protocol().type()),
//                         it->endpoint().protocol().protocol(),
//                         it->host_name(),
//                         nb::make_tuple(it->endpoint().address().to_string(),
//                                        atoi(it->service_name().c_str())
//                                        // TODO: ipv6 got 2 extra field
//                                        )));
//                 }

//                 py_fut.attr("set_result")(nb::cast(v));
//                 return;
//             } catch (const std::exception &e) {
//                 debug_print("error {}", to_utf8(e.what()));
//                 throw;
//             }
//             return;
//         });
// } catch (error_code &e) {
//     debug_print("error code {}", e.value());
//     py_fut.attr("set_exception")(error_code_to_py_error(e));
// } catch (const std::exception &e) {
//     debug_print("error {}", to_utf8(e.what()));
//     throw;
// }

// debug_print("getaddrinfo return");
// return py_fut;
// }

// nb::object EventLoop::create_server(nb::object protocol_factory,
//                                     nb::object host,
//                                     std::optional<int> port,
//                                     int family,
//                                     int flags,
//                                     std::optional<nb::object> sock,
//                                     int backlog,
//                                     std::optional<nb::object> ssl,
//                                     std::optional<bool> reuse_address,
//                                     std::optional<bool> reuse_port,
//                                     std::optional<bool> keep_alive,
//                                     std::optional<double> ssl_handshake_timeout,
//                                     std::optional<double> ssl_shutdown_timeout,
//                                     bool start_serving) {

//     // UNIX socket
//     if (sock.has_value() && sock.value().attr("family").equal(nb::cast(AF_UNIX))) {
//         if (host.is_none() or port.has_value()) {
//             throw nb::value_error("host/port and sock can not be specified at the same time");
//         }

//         throw std::runtime_error("unix socket server is not implement yet");
//         // tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), port));
//     }

//     auto socket = py_socket.attr("socket")(2, 1, 0);

//     // socket.attr("bind")

//     auto py_fut = create_future();

//     auto setsockopt = socket.attr("setsockopt");

//     if (reuse_port.value_or(false)) {
//         setsockopt(SOL_SOCKET, SO_REUSEADDR, 1);
//     }

// #if OS_POSIX
//     if (reuse_port.value_or(false)) {
//         setsockopt(SOL_SOCKET, SO_REUSEPORT, 1);
//     }
// #endif

//     socket.attr("bind")(nb::make_tuple("127.0.0.1", 40404));
//     socket.attr("setblocking")(false);

//     auto server = Server({socket});

//     auto tcp = TCPServer(*this, server, backlog, ssl, ssl_handshake_timeout,
//     ssl_shutdown_timeout);

//     py_fut.attr("set_result")(server);

//     return py_fut;
// }

// async def create_connection(
//     self,
//     protocol_factory,
//     host=None,
//     port=None,
//     *,
//     ssl=None,
//     family=0,
//     proto=0,
//     flags=0,
//     sock=None,
//     local_addr=None,
//     server_hostname=None,
//     ssl_handshake_timeout=None,
//     ssl_shutdown_timeout=None,
//     happy_eyeballs_delay=None,
//     interleave=None,
// ): ...
// nb::object EventLoop ::create_connection(nb::object protocol_factory,
//                                          std::optional<nb::object> host,
//                                          std::optional<nb::object> port,
//                                          std::optional<nb::object> ssl,
//                                          int family,
//                                          int proto,
//                                          int flags,
//                                          std::optional<nb::object> sock,
//                                          std::optional<nb::object> local_addr,
//                                          std::optional<nb::object> server_hostname,
//                                          std::optional<nb::object> ssl_handshake_timeout,
//                                          std::optional<nb::object> ssl_shutdown_timeout,
//                                          std::optional<nb::object> happy_eyeballs_delay,
//                                          std::optional<nb::object> interleave) {

//     // UNIX socket
//     if (sock.has_value() && sock.value().attr("family").equal(nb::cast(AF_UNIX))) {
//         if (host.has_value() or port.has_value()) {
//             throw nb::value_error("host/port and sock can not be specified at the same time");
//         }

//         // tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), port));
//     }

//     using asio::ip::tcp;
//     // acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
//     //     if (!ec) {
//     //         socket;
//     //     }

//     //     do_accept();
//     // });

//     return nb::none();
// }

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
// object EventLoop::sock_sendfile(object sock, object file, int offset, int count, bool fallback) {
//     THROW_NOT_IMPLEMENT;
//     return object();
// }

// // TODO: implement this
// object EventLoop::start_tls(object transport, object protocol, object sslcontext, bool
// server_side,
//                             object server_hostname, object ssl_handshake_timeout) {
//     PyErr_SetString(PyExc_NotImplementedError, "Not implemented!");
//     throw_error_already_set();
//     return object();
// }

// TODO: use asio resolver
// nb::object
// EventLoop::getaddrinfo(nb::object host, int port, int family, int type, int proto, int flags)

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

// nb::object EventLoop::create_future()

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
