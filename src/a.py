import asyncio
import asyncio.futures
import socket
from concurrent.futures import ThreadPoolExecutor

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()


print(socket.getaddrinfo("github.com", 443))


async def main() -> None:
    loop = asyncio.get_event_loop()
    loop.set_default_executor(ThreadPoolExecutor())

    result = await loop.run_in_executor(None, socket.getaddrinfo, "github.com", 443)

    assert result

    print("async main done")


loop.run_until_complete(main())
print("done")
