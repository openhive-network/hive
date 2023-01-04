#!/usr/bin/python3

import json
import sys

import requests

# HIVE node must work
url = "http://127.0.0.1:8090"


def check(item, positive):
    if positive:
        assert "delayed_votes" in item
    else:
        assert "delayed_votes" not in item


def check_in_array(content, positive):
    for item in content:
        check(item, positive)
        break


def query(payload, positive):
    result = requests.post(url, data=payload)
    content = result.json()["result"]

    if type(content) == list:
        check_in_array(content, positive)
    else:
        assert "accounts" in content
        content = content["accounts"]
        check_in_array(content, positive)


if __name__ == "__main__":

    try:

        query('{"jsonrpc":"2.0", "method":"condenser_api.get_accounts", "params":[["initminer"] ], "id":1}', True)
        query('{"jsonrpc":"2.0", "method":"condenser_api.get_accounts", "params":[["initminer"], true ], "id":1}', True)
        query(
            '{"jsonrpc":"2.0", "method":"condenser_api.get_accounts", "params":[["initminer"], false ], "id":1}', False
        )
        print("test {0} passed ".format("condenser_api.get_accounts passed"))

        query(
            '{"jsonrpc":"2.0", "method":"condenser_api.lookup_account_names", "params":[["initminer"]], "id":1}', True
        )
        query(
            '{"jsonrpc":"2.0", "method":"condenser_api.lookup_account_names", "params":[["initminer"], true ], "id":1}',
            True,
        )
        query(
            '{"jsonrpc":"2.0", "method":"condenser_api.lookup_account_names", "params":[["initminer"], false ], "id":1}',
            False,
        )
        print("test {0} passed ".format("condenser_api.lookup_account_names"))

        query(
            '{"jsonrpc":"2.0", "method":"database_api.find_accounts", "params": {"accounts":["initminer"]}, "id":1}',
            True,
        )
        query(
            '{"jsonrpc":"2.0", "method":"database_api.find_accounts", "params": { "accounts":["initminer"], "delayed_votes_active" : true }, "id":1}',
            True,
        )
        query(
            '{"jsonrpc":"2.0", "method":"database_api.find_accounts", "params": { "accounts":["initminer"], "delayed_votes_active" : false }, "id":1}',
            False,
        )
        print("test {0} passed ".format("condenser_api.find_accounts passed"))

        query(
            '{"jsonrpc":"2.0", "method":"database_api.list_accounts", "params": {"start":"initminer", "limit":1, "order":"by_name"}, "id":1}',
            True,
        )
        query(
            '{"jsonrpc":"2.0", "method":"database_api.list_accounts", "params": {"start":"initminer", "limit":1, "order":"by_name", "delayed_votes_active" : true }, "id":1}',
            True,
        )
        query(
            '{"jsonrpc":"2.0", "method":"database_api.list_accounts", "params": {"start":"initminer", "limit":1, "order":"by_name", "delayed_votes_active" : false }, "id":1}',
            False,
        )
        print("test {0} passed ".format("condenser_api.list_accounts passed"))

        print("TEST PASSED")

        sys.exit(0)

    except Exception as ex:

        print("TEST FAILED")
        print(ex.content)

        sys.exit(1)
