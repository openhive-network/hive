from __future__ import annotations

from typing import Any

from beekeepy.exceptions.base import InvalidatedStateError


class StateInvalidator:
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        self.__invalidated: InvalidatedStateError | None = None
        """This var is set to None if object is valid. If it set to exception it is thrown on access to object members after invalidation"""

        self.__objects_to_invalidate: list[StateInvalidator] = []
        super().__init__(*args, **kwargs)

    def invalidate(self, exception: InvalidatedStateError | None = None) -> None:
        exception = exception or InvalidatedStateError()
        for obj in self.__objects_to_invalidate:
            obj.invalidate(exception=exception)
        self.__invalidated = exception

    def register_invalidable(self, obj: StateInvalidator) -> None:
        self.__objects_to_invalidate.append(obj)

    def __getattribute__(self, name: str) -> Any:
        if name != "_StateInvalidator__invalidated" and self.__invalidated is not None:
            raise self.__invalidated
        return super().__getattribute__(name)

    def __setattr__(self, name: str, value: Any) -> None:
        if name != "_StateInvalidator__invalidated" and self.__invalidated is not None:
            raise self.__invalidated
        return super().__setattr__(name, value)
