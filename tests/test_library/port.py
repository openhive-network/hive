import random


class Port:
    @staticmethod
    def __is_port_free(port):
        # Implementation from: https://stackoverflow.com/a/52872579
        import socket
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            return s.connect_ex(('localhost', port)) != 0

    @classmethod
    def allocate(cls):
        """Allocates port which is surely free in the moment of allocation"""
        while True:
            port = random.randint(49152, 65535)
            if cls.__is_port_free(port):
                return port
