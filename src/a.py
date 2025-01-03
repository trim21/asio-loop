import asyncio

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()


class EchoServer(asyncio.Protocol):
    def connection_made(self, transport: asyncio.Transport) -> None:
        addr = transport.get_extra_info("peername")
        print("connection from {}".format(addr))
        self.transport = transport

    def data_received(self, data: bytes) -> None:
        print("data received: {}".format(data.decode()))
        self.transport.write(data)
        self.transport.close()


coro = loop.create_server(EchoServer, "127.0.0.1", 40404)
server = loop.run_until_complete(coro)
print("serving on {}".format(server.sockets[0]))

try:
    loop.run_forever()
except KeyboardInterrupt:
    print("exit")
finally:
    server.close()
    loop.close()
