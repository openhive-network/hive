import requests
import json


def request(url: str, message: dict, max_attempts=3, seconds_between_attempts=0.2):
    assert max_attempts > 0

    message = bytes(json.dumps(message), "utf-8") + b"\r\n"

    response = None
    attempts_left = max_attempts
    while attempts_left > 0:
        result = requests.post(url, data=message)

        success = result.status_code == 200
        response = json.loads(result.text)

        if not success or 'error' in response:
            if attempts_left > 0:
                import time
                time.sleep(seconds_between_attempts)
            attempts_left -= 1
            continue

        return success, response

    raise Exception(f'Problem occurred during communication with {url}.\nSent: {message}.\nReceived: {response}')
