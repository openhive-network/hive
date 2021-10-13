from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import threading
from typing import Any, Optional


class NodeHttpServer:
    __ADDRESS = ('127.0.0.1', 0)

    class __HttpServer(HTTPServer):
        def __init__(self, server_address, request_handler_class, parent):
            super().__init__(server_address, request_handler_class)
            self.parent = parent

        def notify(self, message):
            self.parent.notify(message)

    def __init__(self, observer):
        self.__observer = observer

        self.__server = None
        self.__thread: Optional[threading.Thread] = None

    @property
    def port(self) -> int:
        if self.__server is None:
            self.__server = self.__HttpServer(self.__ADDRESS, HttpRequestHandler, self)

        return self.__server.server_port

    def run(self):
        if self.__thread is not None:
            raise RuntimeError('Server is already running')

        if self.__server is None:
            self.__server = self.__HttpServer(self.__ADDRESS, HttpRequestHandler, self)

        self.__thread = threading.Thread(target=self.__thread_function)
        self.__thread.start()

    def __thread_function(self):
        self.__server.serve_forever()

    def close(self):
        if self.__thread is None:
            return

        self.__server.shutdown()
        self.__server.server_close()
        self.__server = None

        self.__thread.join(timeout=2.0)
        self.__thread = None

    def notify(self, message):
        """Should be called only by request handler when request is received"""
        self.__observer.notify(message)


class HttpRequestHandler(BaseHTTPRequestHandler):
    server: NodeHttpServer

    def do_PUT(self):  # pylint: disable=invalid-name
        content_length = int(self.headers['Content-Length'])
        message = self.rfile.read(content_length)
        self.server.notify(json.loads(message, encoding='utf-8'))

        self.__set_response()

    def log_message(self, format: str, *args: Any) -> None:  # pylint: disable=redefined-builtin
        # This method is defined to silent logs printed after each received message.
        # Solution based on: https://stackoverflow.com/a/3389505
        pass

    def __set_response(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(f'PUT request for {self.path}'.encode('utf-8'))
