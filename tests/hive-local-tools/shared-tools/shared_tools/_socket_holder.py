import socket

from filelock import FileLock


class _SocketHolder:
    def __init__(self):
        self.socket = self.__hold_socket()
        self.ip = self.socket.getsockname()[0]
        self.port = self.socket.getsockname()[1]
        self.address = self.__get_address()

    def __hold_socket(self, address=None):
        lock = FileLock("hold_sockets.txt.lock")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        with lock:
            sock.bind(('localhost', 0) if address is None else address)
            sock.listen(0)
        return sock

    def __get_address(self):
        ip, port = self.ip, self.port
        address = f"{ip}:{port}"
        return address

    def close(self):
        self.socket.close()

    def restart(self):
        self.socket = self.__hold_socket(address=(self.ip, self.port))
