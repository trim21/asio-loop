import asyncio
import socket
import ssl
from collections.abc import Sequence
from typing import Callable, TypeAlias

_ProtocolFactory: TypeAlias = Callable[[], asyncio.BaseProtocol]
_SSLContext: TypeAlias = bool | None | ssl.SSLContext

class Server:
    def __init__(
        self,
        loop: EventLoop,
        sockets: list[socket.socket],
        protocol_factory: _ProtocolFactory,
        ssl: _SSLContext,
        backlog: int,
        ssl_handshake_timeout: None | float,
        ssl_shutdown_timeout: None | float,
    ) -> None: ...

class EventLoop:
    def __init__(self) -> None: ...
    def close(self) -> None: ...
    def is_closed(self) -> bool: ...
    def create_future(self) -> asyncio.Future: ...
    def create_task(self) -> asyncio.Task: ...
    def call_soon(self, callback: Callable, *args, context=None) -> None: ...
    def run_forever(self) -> None: ...
    def run_until_complete(self, future) -> None: ...
    def get_debug(self) -> bool: ...
    def set_debug(self, enabled: bool) -> None: ...
    def call_exception_handler(self, context) -> None: ...
    def stop(self) -> None: ...
    async def getaddrinfo(self, host, port, *, family=0, type=0, proto=0, flags=0): ...
    async def getnameinfo(self, sockaddr, flags=0): ...
    def call_soon_threadsafe(self, callback, *args, context=None): ...
    #
    async def create_server(
        self,
        protocol_factory: _ProtocolFactory,
        host: str | Sequence[str] | None = None,
        port: int = ...,
        *,
        family: int = ...,
        flags: int = ...,
        sock: None = None,
        backlog: int = 100,
        ssl: _SSLContext = None,
        reuse_address: bool | None = None,
        reuse_port: bool | None = None,
        keep_alive: bool | None = None,
        ssl_handshake_timeout: float | None = None,
        ssl_shutdown_timeout: float | None = None,
        start_serving: bool = True,
    ) -> Server: ...
    def _start_serving(self, server):
        pass

    def _stop_serving(self, server):
        pass
