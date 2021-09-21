import atexit
from typing import List, Optional, TYPE_CHECKING
import warnings

from test_tools.private.logger.logger_wrapper import LoggerWrapper
from test_tools.private.scope.context_internal_interface import ContextInternalHandle
from test_tools.private.scope.scope import Scope
from test_tools.private.utilities.disabled_keyboard_interrupt import DisabledKeyboardInterrupt
from test_tools.private.utilities.tests_type import is_manual_test

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
        logger.log_to_stdout()
        root_scope.context.set_logger(logger)

        atexit.register(self.__terminate)

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


current_scope = ScopesStack()
context = ContextInternalHandle(current_scope)

if is_manual_test():
    # Break import-cycle
    from test_tools.private.scope.scoped_current_directory import ScopedCurrentDirectory
    ScopedCurrentDirectory(current_scope.context.get_current_directory())

    root_logger = current_scope.context.get_logger()
    current_scope.context.set_logger(root_logger.create_child_logger('main'))
