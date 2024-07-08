from helpy import HttpUrl
from helpy.exceptions import HelpyError

class BeekeepyError(HelpyError):
    """Base class for all exception raised by beekeepy."""

class BeekeeperAlreadyRunningError(BeekeepyError):
    def __init__(self, *args: object, address: HttpUrl, pid: int) -> None:
        super().__init__(*args)
        self.address = address
        self.pid = pid
