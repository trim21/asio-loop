import asyncio

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()


async def main() -> None:
    loop = asyncio.get_event_loop()

    result = await loop.run_in_executor(
        None, lambda *s: print(repr(s)) or 1, "hello", "world"
    )

    assert result == 1
    print("async main done")


loop.run_until_complete(main())
print("done")
