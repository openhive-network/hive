#!/usr/bin/python3

import sys
import os
import tempfile
import argparse
from threading import Thread

sys.path.append("../../")

import hive_utils
from hive_utils.resources.configini import config as configuration
from hive_utils.resources.configini import validate_address

#################### SETUP

# https://developers.hive.io/tutorials-recipes/paginated-api-methods.html#account_history_apiget_account_history
MAX_AT_ONCE = 10000

parser = argparse.ArgumentParser()
parser.add_argument("--run-hived", dest="hived", help = "Path to hived executable or IP address to replayed node", required=True, type=str)
parser.add_argument("--block-log", dest="block_log_path", help = "Path to block log", required=True, type=str, default=None)
parser.add_argument("--blocks", dest="blocks", help = "Blocks to replay", required=False, type=int, default=3100000)
parser.add_argument("--leave", dest="leave", action='store_true', help="if given, datadir will not be deleted")
parser.add_argument("--mute", dest="mute", action='store_true', help="if given, output from node won't be displayed")

args = parser.parse_args()
node = None

assert int(args.blocks) >= 1000000, "replay has to be done for more than 1 million blocks"

# config
config = configuration()
config.witness = None	# no witness
config.private_key = None	# also no prv key
config.update_plugins("account_by_key account_by_key_api condenser_api account_history_rocksdb account_history_api block_api chain_api".split(" "))
config.exclude_plugins(["witness"])

# working dir
from uuid import uuid5, NAMESPACE_URL
from random import randint
work_dir = os.path.join( tempfile.gettempdir(), uuid5(NAMESPACE_URL,str(randint(0, 1000000))).__str__().replace("-", ""))
os.mkdir(work_dir)

# config paths
config_file_name = 'config.ini'
path_to_config = os.path.join(work_dir, config_file_name)

# setting up block log
blockchain_dir = os.path.join(work_dir, "blockchain")
os.mkdir( blockchain_dir )
assert args.block_log_path
os.symlink(args.block_log_path, os.path.join(blockchain_dir, "block_log"))

# config generation
config.generate(path_to_config)

# setup for replay
node = hive_utils.hive_node.HiveNode(
	args.hived,
	work_dir, 
	["--stop-replay-at-block", str(args.blocks), "--replay-blockchain", "--exit-after-replay"],
	not args.mute
)

print("replaying...")

with node:
	node.wait_till_end()

node.hived_args = ["--stop-replay-at-block", str(args.blocks), "--replay-blockchain" ]
print("replay finished")

#################### BEGIN

class compressed_vop:
	def __init__(self, vop):
		from hashlib import sha512
		from random import randint
		from json import dumps

		self.id = "{}_{}_{}".format( (~0x8000000000000000) & int(vop["operation_id"]), vop["block"], vop["trx_in_block"])
		self.checksum = sha512( dumps(vop).encode() ).hexdigest()
		# self.content = vop

	def get(self):
		return self.__dict__

def compress_vops(data : list) -> list:
	ret = []
	for vop in data:
		ret.append(compressed_vop(vop).get())
	return ret

def get_vops(range_begin : int, range_end : int, start_from_id : int, limit : int) -> dict:
	global config

	from requests import post
	from json import dumps
	
	# from time import sleep
	# sleep(0.25)

	data = {
		"jsonrpc":"2.0",
		"method":"call",
		"params":[
			"account_history_api",
			"enum_virtual_ops",
			{
				"block_range_begin":range_begin,
				"block_range_end":range_end,
				"operation_begin":start_from_id,
				"limit":limit
			}
		],
		"id":1
	}
	
	ret = post(f"http://{config.webserver_http_endpoint}", data=dumps(data))
	if ret.status_code == 200:
		return ret.json()['result']
	else:
		raise Exception("bad request")

def paginated(data : dict, range_end : int) -> bool:
	return not ( data['next_operation_begin'] == 0 and ( data['next_block_range_begin'] == range_end or data['next_block_range_begin'] == 0 ) )

def get_vops_at_once(range_begin : int, range_end : int) -> list:
	tmp = get_vops(range_begin, range_end, 0, MAX_AT_ONCE)
	assert not paginated(tmp, range_end)
	return compress_vops(tmp['ops'])

def get_vops_paginated(range_begin : int, range_end : int, limit : int):
	ret = get_vops(range_begin, range_end, 0, limit)
	yield compress_vops(ret['ops'])
	if not paginated(ret, range_end):
		ret = None
	while ret is not None:
		ret = get_vops(ret['next_block_range_begin'], range_end, ret['next_operation_begin'], limit)
		yield compress_vops(ret['ops'])
		if not paginated(ret, range_end):
			ret = None
	yield None

def get_vops_with_step(range_begin : int, range_end : int, limit : int) -> list:
	next_object = get_vops_paginated(range_begin, range_end, limit)
	ret = []
	value = next(next_object)
	while value is not None:
		ret.extend(value)
		value = next(next_object)
	return ret

def get_vops_one_by_one(range_begin : int, range_end : int) -> list:
	return get_vops_with_step(range_begin, range_end, 1)

def check_range(range_begin : int, blocks : int):
	from operator import itemgetter
	from json import dump
	
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

with node:
	for i in range(args.blocks - (STEP * 4), args.blocks, STEP):
		th = Thread(target=check_range, args=(i, STEP))
		th.start()
		threads.append( th )

	for job in threads:
		job.join()

print("success")

if not args.leave:
	from shutil import rmtree
	rmtree( work_dir )
	print("deleted: {}".format(work_dir))
else:
	print("datadir not deleted: {}".format(work_dir))

exit(0)
