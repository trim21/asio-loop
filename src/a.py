import asyncio
import time

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)


async def main():
    print("async main start")
    loop.call_at(time.time() + 3, lambda: print("hello"))
    # start = time.time()
    # print("start", start)
    await asyncio.sleep(4)
    # await asyncio.gather(asyncio.sleep(2), asyncio.sleep(3))
    # print("end", time.time() - start)
    # # try:
    # #     print(await loop.getaddrinfo("example.com", 80))
    # # except Exception as e:
    # #     print(e)
    print("async main end")


# loop.create_task(main())

# loop.run_forever()
loop.run_until_complete(main())

# gc.collect()

# print("done")
