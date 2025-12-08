import time
import socket
class Synchronizer:
    def __init__(self, master_ip):
        self.master_ip = master_ip
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_TTL, 1)

    def sync_time(self):
        # Request time from master
        self.sock.sendto(b'TIME', (self.master_ip, 5005))
        data, _ = self.sock.recvfrom(1024)
        master_time = float(data.decode())

        # Adjust local time to match master time
        current_time = time.time()
        offset = master_time - current_time
        time.sleep(offset)

if __name__ == '__main__':
    syncer = Synchronizer('192.168.1.1')  # Replace with actual master IP
    syncer.sync_time()