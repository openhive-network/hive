from pathlib import Path
from typing import Optional, Union


class Context:
    DEFAULT_CURRENT_DIRECTORY = Path('./generated').absolute()

    def __init__(self, *, parent: Optional['Context']):
        self.__current_directory: Path

        self.__parent: Optional['Context'] = parent

        if self.__parent is not None:
            self.__current_directory = self.__parent.get_current_directory()
        else:
            self.__current_directory = self.DEFAULT_CURRENT_DIRECTORY

    def get_current_directory(self) -> Path:
        return self.__current_directory

    def set_current_directory(self, directory: Union[str, Path]):
        self.__current_directory = Path(directory)
