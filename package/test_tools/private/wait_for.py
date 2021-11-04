import math
import time
from typing import Optional, TYPE_CHECKING


if TYPE_CHECKING:
    from threading import Event


def wait_for(predicate, *, timeout=math.inf, timeout_error_message=None, poll_time=1.0):
    assert timeout >= 0

    already_waited = 0
    while not predicate():
        if timeout - already_waited <= 0:
            raise TimeoutError(timeout_error_message or 'Waited too long, timeout was reached')

        sleep_time = min(poll_time, timeout)
        time.sleep(sleep_time)
        already_waited += sleep_time

    return already_waited


def wait_for_event(event: 'Event',
                   deadline: Optional[float] = None,
                   exception_message: str = 'The event didn\'t occur within given time frame') -> None:
    """
    Blocks current thread execution until `event` is set. Optionally raises `exception`, when
    `deadline` is reached.

    :param event: Awaited event. When event is set functions stops blocking.
    :param deadline: Time point before which event must occur. Can be counted from the formula:
                     deadline = now + timeout
    :param exception_message: When deadline is reached, TimeoutError with message specified by
                              this parameter will be raised.
    """
    timeout = deadline - time.time()
    if not event.wait(timeout):
        raise TimeoutError(exception_message)
