from __future__ import annotations

import asyncio
from http import HTTPStatus
from typing import Any, Protocol

from aiohttp import web

from .clive_exceptions import CliveError
from .clive_url import Url

JsonT = dict[str, Any]


class AsyncHttpServerError(CliveError):
    pass


class ServerNotRunningError(AsyncHttpServerError):
    def __init__(self) -> None:
        super().__init__("Server is not running. Call run() first.")


class ServerAlreadyRunningError(AsyncHttpServerError):
    def __init__(self) -> None:
        super().__init__("Server is already running. Call close() first.")


class Notifiable(Protocol):
    def notify(self, message: JsonT) -> None: ...


class AsyncHttpServer:
    __ADDRESS = ("127.0.0.1", 0)

    def __init__(self, observer: Notifiable) -> None:
        self.__observer = observer
        self.__app = web.Application()
        self.__app.router.add_route("PUT", "/", self.__do_put)
        self.__site: web.TCPSite | None = None
        self.__is_running: bool = False

    @property
    def port(self) -> int:
        self.__assert_running()
        assert self.__site
        return self.__site._server.sockets[0].getsockname()[1]  # type: ignore[no-any-return, union-attr] 

    @property
    def address(self) -> Url:
        return Url(proto="http", host=self.__ADDRESS[0], port=self.port)

    async def run(self) -> web.AppRunner:
        self.__assert_not_running()
        runner = web.AppRunner(self.__app)
        await runner.setup()
        self.__site = web.TCPSite(runner, *self.__ADDRESS)
        await self.__site.start()
        self.__is_running = True
        while self.__is_running:
            await asyncio.sleep(1)
        await self.__site.stop()
        self.__site = None
        return runner

    def close(self) -> None:
        self.__assert_running()
        assert self.__site
        self.__is_running = False

    async def __do_put(self, request: web.Request) -> web.Response:
        data = request.json()
        self.__observer.notify(await data)
        return web.Response(status=HTTPStatus.NO_CONTENT)

    def __assert_running(self) -> None:
        if not self.__site:
            raise ServerNotRunningError

    def __assert_not_running(self) -> None:
        if self.__site:
            raise ServerAlreadyRunningError
