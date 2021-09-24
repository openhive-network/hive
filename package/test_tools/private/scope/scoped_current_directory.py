from pathlib import Path
from typing import Union

from test_tools.private.scope.scoped_object import ScopedObject
from test_tools.private.scope.scope_singleton import current_scope


class ScopedCurrentDirectory(ScopedObject):
    """
    Scoped object which:
    - on creation -- creates directory and sets it as current directory
    - at exit from scope -- restores previous directory and removes self if is empty
    """

    def __init__(self, path: Union[str, Path]):
        super().__init__()

        self.__path = Path(path)
        self.__create_directory()

        self.__previous_path = current_scope.context.get_current_directory()
        current_scope.context.set_current_directory(self.__path)

    def at_exit_from_scope(self):
        self.__remove_directory_if_empty()
        current_scope.context.set_current_directory(self.__previous_path)

    def __create_directory(self):
        self.__path.mkdir(parents=True, exist_ok=True)

    def __remove_directory_if_empty(self):
        if self.__is_empty():
            self.__path.rmdir()

    def __is_empty(self):
        return not any(self.__path.iterdir())
