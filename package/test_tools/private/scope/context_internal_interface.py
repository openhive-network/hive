from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path

    from test_tools.private.names import Names
    from test_tools.private.scope.context_definition import Context
    from test_tools.private.scope.scope_singleton import ScopesStack


class ContextInternalInterface:
    def __init__(self, scopes_stack: 'ScopesStack'):
        self.__scopes_stack: 'ScopesStack' = scopes_stack

    @property
    def __context(self) -> 'Context':
        return self.__scopes_stack.context

    @property
    def __current_directory(self):
        return self.__context.get_current_directory()

    def get_current_directory(self) -> 'Path':
        assert self.__current_directory.exists()
        return self.__current_directory

    def get_logger(self):
        return self.__context.get_logger()

    @property
    def names(self) -> 'Names':
        return self.__context.get_names()
