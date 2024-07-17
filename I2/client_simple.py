import socket

HOST = "10.200.81.241"  # The server's hostname or IP address
PORT = 50000  # The port used by the server
measured_times = []
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    s.sendall(b"Hello, world!")
    print(str(s.recv(1024)))