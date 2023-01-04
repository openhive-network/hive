#!/usr/bin/python3

import argparse
import os
import sys
import tempfile
from threading import Thread

sys.path.append("../../../")

import hive_utils
from hive_utils.resources.configini import config as configuration
from hive_utils.resources.configini import validate_address

# https://developers.hive.io/tutorials-recipes/paginated-api-methods.html#account_history_apiget_account_history
MAX_AT_ONCE = 10000

parser = argparse.ArgumentParser()
parser.add_argument("--run-hived", dest="hived", help="IP address to replayed node", required=True, type=str)
parser.add_argument(
    "--path-to-config", dest="config_path", help="Path to node config file", required=True, type=str, default=None
)
parser.add_argument("--blocks", dest="blocks", help="Blocks to replay", required=False, type=int, default=1000000)

args = parser.parse_args()
node = None

assert int(args.blocks) >= 1000000, "replay has to be done for more than 1 million blocks"

# config
config = configuration()
config.load(args.config_path)

# check existance of required plugins
plugins = config.plugin.split(" ")
assert "account_history_rocksdb" in plugins
assert "account_history_api" in plugins

# class that compressing vop
class compressed_vop:
    def __init__(self, vop):
        from hashlib import sha512
        from json import dumps
        from random import randint

        self.id = "{}_{}_{}".format((~0x8000000000000000) & int(vop["operation_id"]), vop["block"], vop["trx_in_block"])
        self.checksum = sha512(dumps(vop).encode()).hexdigest()
        # self.content = vop

    def get(self):
        return self.__dict__


# return compressed data from api call
def compress_vops(data: list) -> list:
    ret = []
    for vop in data:
        ret.append(compressed_vop(vop).get())
    return ret


# this function do call to API
def get_vops(range_begin: int, range_end: int, start_from_id: int, limit: int) -> dict:
    global config

    from json import dumps

    from requests import post

    # from time import sleep
    # sleep(0.25)

    data = {
        "jsonrpc": "2.0",
        "method": "call",
        "params": [
            "account_history_api",
            "enum_virtual_ops",
            {
                "block_range_begin": range_begin,
                "block_range_end": range_end,
                "operation_begin": start_from_id,
                "limit": limit,
            },
        ],
        "id": 1,
    }

    ret = post(f"http://{config.webserver_http_endpoint}", data=dumps(data))
    if ret.status_code == 200:
        return ret.json()["result"]
    else:
        raise Exception("bad request")


# checks is there anythink more to get
def paginated(data: dict, range_end: int) -> bool:
    return not (
        data["next_operation_begin"] == 0
        and (data["next_block_range_begin"] == range_end or data["next_block_range_begin"] == 0)
    )


# do one huge call
def get_vops_at_once(range_begin: int, range_end: int) -> list:
    tmp = get_vops(range_begin, range_end, 0, MAX_AT_ONCE)
    assert not paginated(tmp, range_end)
    return compress_vops(tmp["ops"])


# generator, that get page by page in step given as limit
def get_vops_paginated(range_begin: int, range_end: int, limit: int):
    ret = get_vops(range_begin, range_end, 0, limit)
    yield compress_vops(ret["ops"])
    if not paginated(ret, range_end):
        ret = None
    while ret is not None:
        ret = get_vops(ret["next_block_range_begin"], range_end, ret["next_operation_begin"], limit)
        yield compress_vops(ret["ops"])
        if not paginated(ret, range_end):
            ret = None
    yield None


# wrapper on generator that agregates paginated output
def get_vops_with_step(range_begin: int, range_end: int, limit: int) -> list:
    next_object = get_vops_paginated(range_begin, range_end, limit)
    ret = []
    value = next(next_object)
    while value is not None:
        ret.extend(value)
        value = next(next_object)
    return ret


# proxy, to get_vops_with_step with limit set as 1
def get_vops_one_by_one(range_begin: int, range_end: int) -> list:
    return get_vops_with_step(range_begin, range_end, 1)


# get same data in given range with diffrent step
def check_range(range_begin: int, blocks: int):
    from json import dump
    from operator import itemgetter

    range_end = range_begin + blocks + 1

    print(f"gathering blocks in range [ {range_begin} ; {range_end} )")

    all_at_once = get_vops_at_once(range_begin, range_end)
    paginated_by_1 = get_vops_one_by_one(range_begin, range_end)
    paginated_by_2 = get_vops_with_step(range_begin, range_end, 2)
    paginated_by_5 = get_vops_with_step(range_begin, range_end, 5)
    paginated_by_10 = get_vops_with_step(range_begin, range_end, 10)

    # dump(all_at_once, open("all_at_once.json", 'w'))
    # dump(paginated_by_1, open("paginated_by_1.json", 'w'))
    # dump(paginated_by_2, open("paginated_by_2.json", 'w'))
    # dump(paginated_by_5, open("paginated_by_5.json", 'w'))
    # dump(paginated_by_10, open("paginated_by_10.json", 'w'))

    assert all_at_once == paginated_by_1
    print(f"[OK] all == paginated by 1 [ {range_begin} ; {range_end} )")
    assert all_at_once == paginated_by_2
    print(f"[OK] all == paginated by 2 [ {range_begin} ; {range_end} )")
    assert all_at_once == paginated_by_5
    print(f"[OK] all == paginated by 5 [ {range_begin} ; {range_end} )")
    assert all_at_once == paginated_by_10
    print(f"[OK] all == paginated by 10 [ {range_begin} ; {range_end} )")

    return True


threads = []
STEP = 100

# start tests in diffrent threads
for i in range(args.blocks - (STEP * 4), args.blocks, STEP):
    th = Thread(target=check_range, args=(i, STEP))
    th.start()
    threads.append(th)

for job in threads:
    job.join()

print("success")

exit(0)
