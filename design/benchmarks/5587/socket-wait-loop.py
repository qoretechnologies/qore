# Copyright (C) 2026 Qore Technologies, s.r.o.

# a socket ping-pong between two threads over one connection, in which every receive waits; the counterpart of
# socket-wait-loop.qr
import socket
import sys
import threading
import time

N = int(sys.argv[1]) if len(sys.argv) > 1 else 20000

l = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
l.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
l.bind(("127.0.0.1", 0))
l.listen()
c = socket.create_connection(("127.0.0.1", l.getsockname()[1]))
s, _ = l.accept()


def recv_exactly(sock, n):
    buf = b""
    while len(buf) < n:
        buf += sock.recv(n - len(buf))
    return buf


def server():
    for _ in range(N + 1000):
        recv_exactly(s, 4)
        s.sendall(b"pong")


t = threading.Thread(target=server)
t.start()
for _ in range(1000):
    c.sendall(b"ping")
    recv_exactly(c, 4)
start = time.perf_counter()
for _ in range(N):
    c.sendall(b"ping")
    recv_exactly(c, 4)
us = (time.perf_counter() - start) * 1e6
print("%.2f us per round trip" % (us / N))
t.join()
c.close()
s.close()
l.close()
