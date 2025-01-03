import asyncio

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()


async def main() -> int:
    return 1


assert loop.run_until_complete(main()) == 1
print("done")
