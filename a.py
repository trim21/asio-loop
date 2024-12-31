import asyncio
from asioloop import EventLoop


loop: asyncio.BaseEventLoop = EventLoop("ab")

print(loop)
