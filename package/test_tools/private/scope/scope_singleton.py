from test_tools.private.scope.context_internal_interface import ContextInternalHandle
from test_tools.private.scope.scopes_stack import ScopesStack
from test_tools.private.utilities.tests_type import is_manual_test


current_scope = ScopesStack()
context = ContextInternalHandle(current_scope)

if is_manual_test():
    # Break import-cycle
    from test_tools.private.scope.scoped_current_directory import ScopedCurrentDirectory
    ScopedCurrentDirectory(current_scope.context.get_current_directory())

    root_logger = current_scope.context.get_logger()
    current_scope.context.set_logger(root_logger.create_child_logger('main'))

    root_logger.log_to_stdout()
