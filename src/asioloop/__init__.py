from __future__ import annotations

import asyncio
import asyncio.log
import errno
import os
import socket
import ssl
import sys
from asyncio import tasks
from collections.abc import Iterable, Sequence
from itertools import chain
from ssl import SSLContext

from asioloop.__asioloop import EventLoop as _EventLoop
from asioloop.common_types import ProtocolFactory, ServerSSLContext
from asioloop.server import Server

__all__ = ["EventLoop", "AsioEventLoopPolicy", "Server"]


os_name = os.name
sys_platform = sys.platform

SOCK_STREAM = socket.SOCK_STREAM
SOCK_NONBLOCK: int = getattr(socket, "SOCK_NONBLOCK", -1)

has_SO_REUSEPORT = hasattr(socket, "SO_REUSEPORT")
has_IPV6_V6ONLY = hasattr(socket, "IPV6_V6ONLY")
_HAS_IPv6 = hasattr(socket, "AF_INET6")


def _is_sock_stream(sock_type: int):
    if SOCK_NONBLOCK == -1:
        return sock_type == socket.SOCK_STREAM
    else:
        # Linux's socket.type is a bitmask that can include extra info
        # about socket (like SOCK_NONBLOCK bit), therefore we can't do simple
        # `sock_type == socket.SOCK_STREAM`, see
        # https://github.com/torvalds/linux/blob/v4.13/include/linux/net.h#L77
        # for more details.
        return (sock_type & 0xF) == socket.SOCK_STREAM


logger = asyncio.log.logger


def _check_ssl_socket(sock):
    if ssl is not None and isinstance(sock, ssl.SSLSocket):
        raise TypeError("Socket cannot be of type SSLSocket")


def _set_reuseport(sock):
    if not has_SO_REUSEPORT:
        raise ValueError("reuse_port not supported by socket module")
    else:
        try:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        except OSError:
            raise ValueError(
                "reuse_port not supported by socket module, "
                "SO_REUSEPORT defined but not implemented."
            )


class EventLoop(_EventLoop):
    @property
    def _debug(self) -> bool:
        return self.get_debug()

    # we can't really implement a coroutine in c-level, raw future is eager executed.
    # so we need to wrap it in to python coroutine to make it
    async def create_server(
        self,
        protocol_factory: ProtocolFactory,
        host: str | Sequence[str] | None = None,
        port: int = 0,
        *,
        family: int = socket.AddressFamily.AF_UNSPEC,
        flags: int = socket.AddressInfo.AI_PASSIVE,
        sock: socket.socket | None = None,
        backlog: int = 100,
        ssl: ServerSSLContext = None,
        reuse_address: bool | None = None,
        reuse_port: bool | None = None,
        keep_alive: bool | None = None,
        ssl_handshake_timeout: float | None = None,
        ssl_shutdown_timeout: float | None = None,
        start_serving: bool = True,
    ) -> Server:
        if ssl is not None:
            if isinstance(ssl, bool):
                raise TypeError(
                    "ssl argument must be an SSLContext or None, got {} instead".format(
                        ssl
                    )
                )
            if not isinstance(ssl, SSLContext):
                raise TypeError(
                    "ssl argument must be an SSLContext or None, got {} instead".format(
                        ssl
                    )
                )

        if ssl_handshake_timeout is not None and ssl is None:
            raise ValueError("ssl_handshake_timeout is only meaningful with ssl")

        if ssl_shutdown_timeout is not None and ssl is None:
            raise ValueError("ssl_shutdown_timeout is only meaningful with ssl")

        if sock is not None:
            _check_ssl_socket(sock)

        if host is not None or port is not None:
            if sock is not None:
                raise ValueError(
                    "host/port and sock can not be specified at the same time"
                )

            if reuse_address is None:
                reuse_address = os.name == "posix" and sys.platform != "cygwin"
            sockets = []
            if host == "":
                hosts = [None]
            elif isinstance(host, str) or not isinstance(host, Iterable):
                hosts = [host]
            else:
                hosts = host

            fs = [
                self._create_server_getaddrinfo(host, port, family=family, flags=flags)
                for host in hosts
            ]
            infos = await tasks.gather(*fs)
            infos = set(chain.from_iterable(infos))

            completed = False
            try:
                for res in infos:
                    af, socktype, proto, canonname, sa = res
                    try:
                        sock = socket.socket(af, socktype, proto)
                    except OSError:
                        # Assume it's a bad family/type/protocol combination.
                        if self._debug:
                            logger.warning(
                                "create_server() failed to create "
                                "socket.socket(%r, %r, %r)",
                                af,
                                socktype,
                                proto,
                                exc_info=True,
                            )
                        continue
                    sockets.append(sock)
                    if reuse_address:
                        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)
                    if reuse_port:
                        _set_reuseport(sock)
                    if keep_alive:
                        sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, True)
                    # Disable IPv4/IPv6 dual stack support (enabled by
                    # default on Linux) which makes a single socket
                    # listen on both address families.
                    if (
                        _HAS_IPv6
                        and af == socket.AF_INET6
                        and hasattr(socket, "IPPROTO_IPV6")
                    ):
                        sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, True)
                    try:
                        sock.bind(sa)
                    except OSError as err:
                        msg = (
                            "error while attempting "
                            "to bind on address {!r}: {}".format(
                                sa,
                                str(err).lower(),
                            )
                        )
                        if err.errno == errno.EADDRNOTAVAIL:
                            # Assume the family is not enabled (bpo-30945)
                            sockets.pop()
                            sock.close()
                            if self.get_debug():
                                logger.warning(msg)
                            continue
                        raise OSError(err.errno, msg) from None

                if not sockets:
                    raise OSError(
                        "could not bind on any address out of {!r}".format(
                            [info[4] for info in infos]
                        )
                    )

                completed = True
            finally:
                if not completed:
                    for sock in sockets:
                        sock.close()
        else:
            if sock is None:
                raise ValueError("Neither host/port nor sock were specified")
            if sock.type != socket.SOCK_STREAM:
                raise ValueError(f"A Stream Socket was expected, got {sock!r}")
            sockets = [sock]

        for sock in sockets:
            sock.setblocking(False)

        server = Server(
            self,
            sockets,
            protocol_factory,
            ssl,
            backlog,
            ssl_handshake_timeout,
            ssl_shutdown_timeout,
        )
        if start_serving:
            server._start_serving()
            # Skip one loop iteration so that all 'loop.add_reader'
            # go through.
            await tasks.sleep(0)

        if self._debug:
            logger.info("%r is serving", server)

        return await super().create_server(
            protocol_factory=protocol_factory,
            host=host,
            port=port,
            family=family,
            flags=flags,
            sock=sock,
            backlog=backlog,
            ssl=ssl,
            reuse_address=reuse_address,
            reuse_port=reuse_port,
            keep_alive=keep_alive,
            ssl_handshake_timeout=ssl_handshake_timeout,
            ssl_shutdown_timeout=ssl_shutdown_timeout,
            start_serving=start_serving,
        )  # type: ignore

    async def _create_server_getaddrinfo(self, host, port, family, flags):
        infos = await self._ensure_resolved(
            (host, port), family=family, type=socket.SOCK_STREAM, flags=flags, loop=self
        )
        if not infos:
            raise OSError(f"getaddrinfo({host!r}) returned empty list")
        return infos

    async def _ensure_resolved(
        self, address, *, family=0, type=socket.SOCK_STREAM, proto=0, flags=0, loop
    ):
        host, port = address[:2]
        info = _ipaddr_info(host, port, family, type, proto, *address[2:])
        if info is not None:
            # "host" is already a resolved IP.
            return [info]
        else:
            return await loop.getaddrinfo(
                host, port, family=family, type=type, proto=proto, flags=flags
            )


class AsioEventLoopPolicy(asyncio.events.BaseDefaultEventLoopPolicy):
    _loop_factory = EventLoop

    def set_event_loop(self, loop: EventLoop) -> None:
        """Set the event loop."""
        self._local._set_called = True
        if loop is not None and not isinstance(loop, EventLoop):
            raise TypeError(
                f"loop must be an instance of EventLoop or None, not '{type(loop).__name__}'"
            )
        self._local._loop = loop


def _ipaddr_info(host, port, family, type, proto, flowinfo=0, scopeid=0):
    # Try to skip getaddrinfo if "host" is already an IP. Users might have
    # handled name resolution in their own code and pass in resolved IPs.
    if not hasattr(socket, "inet_pton"):
        return

    if proto not in {0, socket.IPPROTO_TCP, socket.IPPROTO_UDP} or host is None:
        return None

    if type == socket.SOCK_STREAM:
        proto = socket.IPPROTO_TCP
    elif type == socket.SOCK_DGRAM:
        proto = socket.IPPROTO_UDP
    else:
        return None

    if port is None:
        port = 0
    elif isinstance(port, bytes) and port == b"":
        port = 0
    elif isinstance(port, str) and port == "":
        port = 0
    else:
        # If port's a service name like "http", don't skip getaddrinfo.
        try:
            port = int(port)
        except (TypeError, ValueError):
            return None

    if family == socket.AF_UNSPEC:
        afs = [socket.AF_INET]
        if _HAS_IPv6:
            afs.append(socket.AF_INET6)
    else:
        afs = [family]

    if isinstance(host, bytes):
        host = host.decode("idna")
    if "%" in host:
        # Linux's inet_pton doesn't accept an IPv6 zone index after host,
        # like '::1%lo0'.
        return None

    for af in afs:
        try:
            socket.inet_pton(af, host)
            # The host has already been resolved.
            if _HAS_IPv6 and af == socket.AF_INET6:
                return af, type, proto, "", (host, port, flowinfo, scopeid)
            else:
                return af, type, proto, "", (host, port)
        except OSError:
            pass

    # "host" is not an IP address.
    return None
