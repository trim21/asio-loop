import asyncio
import time

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()


async def main() -> None:
    start = time.monotonic()
    await asyncio.sleep(5)
    end = time.monotonic()
    assert end - start >= 4


loop.run_until_complete(main())
print("done")
