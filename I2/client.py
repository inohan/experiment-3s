import socket
import random
import time
import numpy as np
from matplotlib import pyplot as plt

HOST = "10.200.81.241"  # The server's hostname or IP address
PORT = 50000  # The port used by the server
measured_times = []
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    for _ in range(10000):
        string_send = "".join([str(random.randint(0, 9)) for _ in range(1024)])
        start = time.time()
        s.sendall(bytes(string_send, "ascii"))
        data = s.recv(1024)
        end = time.time()
        measured_times.append(end-start)
lag_frame = np.array(measured_times)*44100/2
print("Avg.:", np.mean(lag_frame))
print("Stdev.:", np.std(lag_frame))
print("1%:", np.percentile(lag_frame, 5))
print("25%:", np.percentile(lag_frame, 25))
print("50%:", np.percentile(lag_frame, 50))
print("75%:", np.percentile(lag_frame, 75))
print("99%:", np.percentile(lag_frame, 95))

plt.title("TCP lag")
plt.ylabel("Lag (frames)")
plt.boxplot(lag_frame)
plt.savefig("TCP_lag.png")
plt.show()