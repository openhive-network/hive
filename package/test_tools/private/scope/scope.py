from typing import List, Optional, TYPE_CHECKING

from test_tools.private.scope.context_definition import Context

if TYPE_CHECKING:
    from test_tools.private.scope import ScopedObject


class Scope:
    def __init__(self, parent: Optional['Scope']):
        self.scoped_objects: List['ScopedObject'] = []
        self.context: Context = Context(parent=parent.context if parent is not None else None)

    def enter(self):
        pass

    def register(self, scoped_object: 'ScopedObject'):
        self.scoped_objects.append(scoped_object)

    def exit(self):
        for scoped_object in reversed(self.scoped_objects):
            scoped_object.at_exit_from_scope()
