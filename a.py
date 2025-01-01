import asyncio
import time

# asyncio.set_event_loop_policy(AsioEventLoopPolicy())

# asyncio.ensure_future

loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)


async def main():
    print("async main start")
    start = time.time()
    print("start", start)
    await asyncio.sleep(3)
    # await asyncio.gather(asyncio.sleep(2), asyncio.sleep(3))
    print("end", time.time() - start)
    # try:
    #     print(await loop.getaddrinfo("example.com", 80))
    # except Exception as e:
    #     print(e)
    print("async main end")


loop.create_task(main())

loop.run_forever()
# loop.run_until_complete(main())

# gc.collect()

# print("done")
