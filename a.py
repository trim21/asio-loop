import socket

with socket.create_connection(("127.0.0.1", 40404)) as conn:
    conn.send(b"hello world\n")
    data = conn.recv(len(b"hello world\n"))
    print(data)
