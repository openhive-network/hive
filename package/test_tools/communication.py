import json
import time

import requests

from test_tools.exceptions import CommunicationError
from test_tools.private.asset import AssetBase


class CustomJsonEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, AssetBase):
            return str(o)

        return super().default(o)


def request(url: str, message: dict, max_attempts=3, seconds_between_attempts=0.2):
    assert max_attempts > 0

    message = bytes(json.dumps(message, cls=CustomJsonEncoder), "utf-8") + b"\r\n"

    attempts_left = max_attempts
    while attempts_left > 0:
        response = requests.post(url, data=message)
        if response.status_code != 200:
            if attempts_left > 0:
                time.sleep(seconds_between_attempts)
            attempts_left -= 1
            continue

        return json.loads(response.text)

    raise CommunicationError(
        f'Problem occurred during communication with {url}',
        message,
        response.text
    )
