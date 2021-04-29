class LackOfPortsError(Exception):
    pass


class PortRange:
    def __init__(self, begin, end):
        self.begin = begin
        self.end = end
        self.next_free = begin

    def allocate_port(self):
        """Allocates port which is surely free in the moment of allocation"""
        while True:
            port = self.next_free
            if port == self.end:
                raise LackOfPortsError('All ports are already in use')

            self.next_free += 1
            if self.__is_port_free(port):
                return port

    def allocate_port_range(self, number_of_ports):
        if self.__get_number_of_free_ports() < number_of_ports:
            raise LackOfPortsError('')

        begin = self.next_free
        self.next_free += number_of_ports
        return PortRange(begin, self.next_free)

    def __get_number_of_free_ports(self):
        return self.end - self.next_free

    @staticmethod
    def __is_port_free(port):
        # Implementation from: https://stackoverflow.com/a/52872579
        import socket
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            return s.connect_ex(('localhost', port)) != 0
