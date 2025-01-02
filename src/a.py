import rich
import socket
import asyncio

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)

rich.print(socket.getaddrinfo("www.baidu.com", 443))

socket.SocketKind


async def main():
    # try:
    rich.print(await loop.getaddrinfo("www.baidu.com", 443))
    print("done")
    # except Exception as e:
    # print("ee", repr(e))


print(loop.run_until_complete(main()))
