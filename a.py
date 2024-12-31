import asyncio
from asioloop import EventLoop


loop: asyncio.BaseEventLoop = EventLoop("a")

task = asyncio.sleep(1, 2)
assert loop.run_until_complete(task) == 2
