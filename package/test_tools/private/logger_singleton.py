# pylint: disable=import-outside-toplevel
# This is required because of bad implementation of logger.
# Hopefully it will be rewritten in near future and then above
# pylint-ignore should be removed.

def __create_logger():
    from test_tools.private.logger import Logger
    return Logger()

logger = __create_logger()
