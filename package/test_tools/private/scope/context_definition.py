from typing import Optional


class Context:
    def __init__(self, *, parent: Optional['Context']):
        self.__parent: Optional['Context'] = parent
