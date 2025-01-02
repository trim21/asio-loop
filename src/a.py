import asyncio
import socket

import rich

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()

rich.print(socket.getaddrinfo("www.baidu.com", 443))


async def main() -> None:
    loop = asyncio.get_event_loop()
    print("hello")

    rich.print(await loop.getaddrinfo("www.baidu.com", 443))
    print(asyncio.get_event_loop())
    print(asyncio.events.get_running_loop())
    await asyncio.sleep(1)


loop.run_until_complete(main())
