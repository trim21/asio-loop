import asyncio
import socket
from pprint import pprint

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()


async def main() -> None:
    loop = asyncio.get_event_loop()

    result = await loop.getaddrinfo("127.0.0.1", 443)
    print("our: ", end="")
    pprint(result)

    socket_result = socket.getaddrinfo("127.0.0.1", 443)
    print("socket: ", end="")
    pprint(socket_result)

    print("async main done")


loop.run_until_complete(main())
print("done")
