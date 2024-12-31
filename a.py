import asyncio
import gc
import socket
from asioloop import EventLoop


loop: asyncio.BaseEventLoop = EventLoop("ab")

print(socket.SOL_TCP)
print(socket.getaddrinfo("example.com", 80))


async def main():
    print("async main ")
    try:
        print(await loop.getaddrinfo("example.com", 80))
    except Exception as e:
        print(e)
    print("async main 2")


loop.run_until_complete(main())

gc.collect()

print("done")
