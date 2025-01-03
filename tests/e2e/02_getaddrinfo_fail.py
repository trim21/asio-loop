import asyncio
import asyncio.futures
import socket
import threading
from concurrent.futures import ThreadPoolExecutor

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.SelectorEventLoop()


def work():
    print("run work in thread", threading.current_thread().native_id)
    print("now try to resolve")
    try:
        result = socket.getaddrinfo("www.baidu.com", 443)
    except Exception as e:
        print(e)
        raise
    print("run work in thread", threading.current_thread().native_id)
    print("resolve done")
    return result


async def main() -> None:
    loop.set_default_executor(ThreadPoolExecutor(thread_name_prefix="asyncio"))

    result = await loop.run_in_executor(None, work)

    print("async main done", result)


loop.run_until_complete(main())
print("done")
