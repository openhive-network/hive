from test_tools.private.scope.scope_singleton import current_scope


class ScopedObject:
    def __init__(self):
        current_scope.register(self)

    def at_exit_from_scope(self):
        pass
