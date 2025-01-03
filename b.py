import asyncio
import inspect

print(inspect.signature(asyncio.ProactorEventLoop().create_server))
