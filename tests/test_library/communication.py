import ast
import requests
import json


def request(url: str, message: dict):
    message = bytes(json.dumps(message), "utf-8") + b"\r\n"

    result = requests.post(url, data=message)

    success = result.status_code == 200
    response = ast.literal_eval(result.text)

    return response if success else None
