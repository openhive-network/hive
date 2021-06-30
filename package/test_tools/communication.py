import requests
import json

from test_tools.exceptions import CommunicationError


def request(url: str, message: dict, max_attempts=3, seconds_between_attempts=0.2):
    assert max_attempts > 0

    message = bytes(json.dumps(message), "utf-8") + b"\r\n"

    attempts_left = max_attempts
    while attempts_left > 0:
        result = requests.post(url, data=message)
        if result.status_code != 200:
            if attempts_left > 0:
                import time
                time.sleep(seconds_between_attempts)
            attempts_left -= 1
            continue

        return json.loads(result.text)

    raise CommunicationError(
        f'Problem occurred during communication with {url}',
        message,
        result.text
    )
