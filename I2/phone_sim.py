import random
class PacketSimulator:
    def __init__(self, tcp_lag_avg = 10, tcp_lag_stdev = 0, read_size = 10) -> None:
        self.tcp_lag_avg = tcp_lag_avg
        self.tcp_lag_stdev = tcp_lag_stdev
        self.read_size = read_size
        self.time = 0
        self.buf_mic = []
        self.buf_recv = []
        self.buf_speaker = []
        self.bool_send = False
        self.bool_recv = False
        self.last_out = 0
        self.logger = []
    
    def advance_time(self):
        if len(self.buf_speaker) > 0:
            self.last_out = self.buf_speaker.pop(0)
        self.logger.append([self.time, self.last_out, len(self.buf_mic), int(self.bool_send), int(self.bool_recv)])
        self.time += 1
        self.bool_recv = False
        self.bool_send = False
        self.buf_mic.append(self.time)

    def send(self):
        while len(self.buf_mic) < self.read_size:
            self.advance_time()
        lag = max(random.gauss(self.tcp_lag_avg, self.tcp_lag_stdev), 1)
        for i in range(self.read_size):
            self.buf_recv.append([self.time + lag, self.buf_mic.pop(0)])
        self.bool_send = True

    def recv(self):
        while len(received := [data for data in self.buf_recv if self.time >= data[0]]) < self.read_size:
            self.advance_time()
        for i in range(self.read_size):
            self.buf_speaker.append(received[i][1])
            self.buf_recv.remove(received[i])
        self.bool_recv = True

if __name__ == "__main__":
    sim = PacketSimulator(tcp_lag_avg=5, read_size=6)
    # for _ in range(1):
    #     sim.send()
    for _ in range(30):
        sim.send()
        sim.recv()
    for _ in range(30):
        sim.advance_time()