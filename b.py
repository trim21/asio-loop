import socket
import asyncio

from asioloop import AsioEventLoopPolicy

asyncio.set_event_loop_policy(AsioEventLoopPolicy())

loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)

# print(socket.getnameinfo(("93.184.216.34", "https"), socket.NI_NOFQDN))


async def main():
    try:
        print(await loop.getnameinfo(("93.184.216.34", "https"), socket.NI_NOFQDN))
    except Exception as e:
        print(repr(e))


loop.run_until_complete(main())
