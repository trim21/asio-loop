import socket
import asyncio

print(socket.getaddrinfo("www.baidu.com"))


loop = asyncio.new_event_loop()


async def main():
    print(await loop.getnameinfo(("93.184.216.34", 443), socket.NI_NOFQDN))


loop.run_until_complete(main())
