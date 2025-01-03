from __future__ import annotations

import socket
import weakref
from asyncio import (
    Transport,
    events,
    exceptions,
    tasks,
)
from typing import TYPE_CHECKING

from asioloop.common_types import ProtocolFactory, ServerSSLContext

if TYPE_CHECKING:
    from asioloop.__asioloop import EventLoop


class TCPServer:
    __slots__ = ("_sock",)

    def __init__(self, sock: socket.socket):
        self._sock = sock

    @property
    def family(self):
        return self._sock.family

    @property
    def type(self):
        return self._sock.type

    @property
    def proto(self):
        return self._sock.proto

    def __repr__(self):
        s = (
            f"<asyncio.TransportSocket fd={self.fileno()}, "
            f"family={self.family!s}, type={self.type!s}, "
            f"proto={self.proto}"
        )

        if self.fileno() != -1:
            try:
                laddr = self.getsockname()
                if laddr:
                    s = f"{s}, laddr={laddr}"
            except OSError:
                pass
            try:
                raddr = self.getpeername()
                if raddr:
                    s = f"{s}, raddr={raddr}"
            except OSError:
                pass

        return f"{s}>"

    def __getstate__(self):
        raise TypeError("Cannot serialize asyncio.TransportSocket object")

    def fileno(self):
        return self._sock.fileno()

    def dup(self):
        return self._sock.dup()

    def get_inheritable(self):
        return self._sock.get_inheritable()

    def shutdown(self, how: int):
        # asyncio doesn't currently provide a high-level transport API
        # to shutdown the connection.
        self._sock.shutdown(how)

    def getsockopt(self, *args, **kwargs):
        return self._sock.getsockopt(*args, **kwargs)

    def setsockopt(self, *args, **kwargs):
        self._sock.setsockopt(*args, **kwargs)

    def getpeername(self):
        return self._sock.getpeername()

    def getsockname(self):
        return self._sock.getsockname()

    def getsockbyname(self):
        return self._sock.getsockbyname()

    def settimeout(self, value: float):
        if value == 0:
            return
        raise ValueError("settimeout(): only 0 timeout is allowed on transport sockets")

    def gettimeout(self):
        return 0

    def setblocking(self, flag: int):
        if not flag:
            return
        raise ValueError("setblocking(): transport sockets cannot be blocking")


class Server(events.AbstractServer):
    def __init__(
        self,
        loop: EventLoop,
        sockets: list[TCPServer],
        protocol_factory: ProtocolFactory,
        ssl_context: ServerSSLContext,
        backlog: int,
        ssl_handshake_timeout: None | float = None,
        ssl_shutdown_timeout: None | float = None,
    ):
        self._loop = loop
        self._sockets: list[TCPServer] = sockets
        # Weak references so we don't break Transport's ability to
        # detect abandoned transports
        self._clients = weakref.WeakSet()
        self._waiters = []
        self._protocol_factory = protocol_factory
        self._backlog = backlog
        self._ssl_context = ssl_context
        self._ssl_handshake_timeout = ssl_handshake_timeout
        self._ssl_shutdown_timeout = ssl_shutdown_timeout
        self._serving = False
        self._serving_forever_fut = None

    def __repr__(self):
        return f"<{self.__class__.__name__} sockets={self.sockets!r}>"

    def _attach(self, transport: Transport):
        assert self._sockets is not None
        self._clients.add(transport)

    def _detach(self, transport: Transport):
        self._clients.discard(transport)
        if len(self._clients) == 0 and self._sockets is None:
            self._wakeup()

    def _wakeup(self):
        waiters = self._waiters
        self._waiters = None
        for waiter in waiters:
            if not waiter.done():
                waiter.set_result(None)

    def _start_serving(self):
        if self._serving:
            return
        self._serving = True
        for sock in self._sockets:
            sock.listen(self._backlog)
            self._loop._start_serving(
                self._protocol_factory,
                sock,
                self._ssl_context,
                self,
                self._backlog,
                self._ssl_handshake_timeout,
                self._ssl_shutdown_timeout,
            )

    def get_loop(self):
        return self._loop

    def is_serving(self):
        return self._serving

    @property
    def sockets(self):
        return self._sockets or ()

    def close(self):
        sockets = self._sockets
        if sockets is None:
            return
        self._sockets = None  # type: ignore

        for sock in sockets:
            self._loop._stop_serving(sock)

        self._serving = False

        if (
            self._serving_forever_fut is not None
            and not self._serving_forever_fut.done()
        ):
            self._serving_forever_fut.cancel()
            self._serving_forever_fut = None

        if len(self._clients) == 0:
            self._wakeup()

    def close_clients(self):
        for transport in self._clients.copy():
            transport.close()

    def abort_clients(self):
        for transport in self._clients.copy():
            transport.abort()

    async def start_serving(self):
        self._start_serving()
        # Skip one loop iteration so that all 'loop.add_reader'
        # go through.
        await tasks.sleep(0)

    async def serve_forever(self):
        if self._serving_forever_fut is not None:
            raise RuntimeError(
                f"server {self!r} is already being awaited on serve_forever()"
            )
        if self._sockets is None:
            raise RuntimeError(f"server {self!r} is closed")

        self._start_serving()
        self._serving_forever_fut = self._loop.create_future()

        try:
            await self._serving_forever_fut
        except exceptions.CancelledError:
            try:
                self.close()
                await self.wait_closed()
            finally:
                raise
        finally:
            self._serving_forever_fut = None

    async def wait_closed(self):
        """Wait until server is closed and all connections are dropped.

        - If the server is not closed, wait.
        - If it is closed, but there are still active connections, wait.

        Anyone waiting here will be unblocked once both conditions
        (server is closed and all connections have been dropped)
        have become true, in either order.

        Historical note: In 3.11 and before, this was broken, returning
        immediately if the server was already closed, even if there
        were still active connections. An attempted fix in 3.12.0 was
        still broken, returning immediately if the server was still
        open and there were no active connections. Hopefully in 3.12.1
        we have it right.
        """
        # Waiters are unblocked by self._wakeup(), which is called
        # from two places: self.close() and self._detach(), but only
        # when both conditions have become true. To signal that this
        # has happened, self._wakeup() sets self._waiters to None.
        if self._waiters is None:
            return
        waiter = self._loop.create_future()
        self._waiters.append(waiter)
        await waiter
