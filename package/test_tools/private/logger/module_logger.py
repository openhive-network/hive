from test_tools.private.logger.logger_wrapper import LoggerWrapper
from test_tools.private.logger.package_logger import PackageLogger


class ModuleLogger(LoggerWrapper):
    def __init__(self, name: str, parent: LoggerWrapper, propagate=True):
        parent = parent if not isinstance(parent, PackageLogger) else parent.parent
        super().__init__(name, parent=parent, propagate=propagate)
