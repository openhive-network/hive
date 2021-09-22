from test_tools.private.scope import context


class ContextHandle:
    @staticmethod
    def get_current_directory():
        return context.get_current_directory()


context_singleton = ContextHandle()
