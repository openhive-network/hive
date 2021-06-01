import math
import time

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
