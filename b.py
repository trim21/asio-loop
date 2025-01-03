# import asyncio

# asyncio.ProactorEventLoop().run_in_executor()

import socket

# socket.create_server()
s = socket.create_server(("127.0.0.1", 4040))
print(s)

socket.fromfd()
