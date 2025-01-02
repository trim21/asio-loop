import asyncio

loop = asyncio.new_event_loop()

loop.shutdown_default_executor()


asyncio.TimerHandle()
