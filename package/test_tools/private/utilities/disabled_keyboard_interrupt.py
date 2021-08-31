# Based on https://stackoverflow.com/a/21919644

import signal


class DisabledKeyboardInterrupt:
    """Context manager to temporarily disable keyboard interrupt (SIGINT signal).

    Usage example with explanation:
    with DisabledKeyboardInterrupt():
        do_something()  # [1]
    # [2]

    If keyboard interrupt will be raised during [1], it will ignored and reraised
    after exiting from covered scope [2].
    """

    def __init__(self):
        self.old_handler = None
        self.signal_received = None

    def __enter__(self):
        self.old_handler = signal.signal(signal.SIGINT, self.handler)

    def handler(self, sig, frame):
        self.signal_received = (sig, frame)

    def __exit__(self, type_, value, traceback):
        signal.signal(signal.SIGINT, self.old_handler)
        if self.signal_received is not None:
            self.old_handler(*self.signal_received)
