import asyncio

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()


async def main() -> None:
    print(loop.time())


loop.run_until_complete(main())
print("done")
