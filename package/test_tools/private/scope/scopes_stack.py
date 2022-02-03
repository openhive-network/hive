import atexit
from typing import List, Optional, TYPE_CHECKING
import warnings

from test_tools.private.logger.logger_wrapper import LoggerWrapper
from test_tools.private.raise_exception_helper import RaiseExceptionHelper
from test_tools.private.scope.scope import Scope
from test_tools.private.utilities.disabled_keyboard_interrupt import DisabledKeyboardInterrupt

if TYPE_CHECKING:
    from test_tools.private.scope import ScopedObject


class ScopesStack:
    class __NamedScope(Scope):
        def __init__(self, name: str, parent: Optional['Scope']):
            super().__init__(parent)

            self.name = name

        def __repr__(self):
            return f'<Scope \'{self.name}\'>'

    class __ScopeContextManager:
        def __init__(self, scope):
            self.__scope = scope

        def __enter__(self):
            pass

        def __exit__(self, exc_type, exc_val, exc_tb):
            self.__scope.exit()

    def __init__(self):
        self.__scopes_stack: List['__NamedScope'] = []
        self.create_new_scope('root')

        root_scope = self.__current_scope
        logger = LoggerWrapper('root', parent=None)
        root_scope.context.set_logger(logger)

        atexit.register(self.__terminate)

        RaiseExceptionHelper.initialize()

    def __repr__(self):
        return f'<ScopesStack: {", ".join(repr(scope) for scope in self.__scopes_stack)}>'

    @property
    def __current_scope(self) -> Scope:
        return self.__scopes_stack[-1] if self.__scopes_stack else None

    def create_new_scope(self, name):
        new_scope = self.__NamedScope(name, parent=self.__current_scope)
        new_scope.enter()
        self.__scopes_stack.append(new_scope)
        return self.__ScopeContextManager(self)

    def register(self, scoped_object: 'ScopedObject'):
        self.__current_scope.register(scoped_object)

    def exit(self):
        with DisabledKeyboardInterrupt():
            self.__current_scope.exit()
            self.__scopes_stack.pop()

    @property
    def context(self):
        return self.__current_scope.context

    def __terminate(self):
        with DisabledKeyboardInterrupt():
            if len(self.__scopes_stack) != 1:
                warnings.warn('You forgot to exit from some scope')

            while len(self.__scopes_stack) != 0:
                self.exit()
