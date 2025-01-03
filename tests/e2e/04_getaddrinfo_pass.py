import asyncio
import asyncio.futures
import socket
import threading
from concurrent.futures import ThreadPoolExecutor

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()

print("main thread", threading.current_thread().native_id)


def work():
    print("run work in thread", threading.current_thread().native_id)
    print("now try to resolve")
    try:
        result = socket.getaddrinfo("www.baidu.com", 443)
    except Exception as e:
        print(e)
        raise
    print("resolve done", result)
    return result


async def main() -> None:
    with ThreadPoolExecutor() as e:
        f = e.submit(work)
        f.add_done_callback(lambda *args: print("source future done"))
        fur = asyncio.futures.wrap_future(f, loop=loop)
        while True:
            await asyncio.sleep(1)
            if fur.done():
                break

    print("async main done", fur.result())


loop.run_until_complete(main())
print("done")
