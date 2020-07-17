#!/usr/bin/python3

import sys
import os
import tempfile
import argparse

sys.path.append("../../")

import hive_utils
from hive_utils.resources.configini import config as configuration
from hive_utils.common import wait_for_node


parser = argparse.ArgumentParser()
parser.add_argument("--run-hived", dest="hived", help = "Path to hived executable", required=True, type=str)
parser.add_argument("--block-log", dest="block_log_path", help = "Path to block log", required=False, type=str, default=None)
parser.add_argument("--blocks", dest="blocks", help = "Blocks to replay", required=False, type=int, default=1000)
parser.add_argument("--leave", dest="leave", action='store_true')

args = parser.parse_args()
node = None

assert args.hived

# working dir
from uuid import uuid5, NAMESPACE_URL
from random import randint
work_dir = os.path.join( tempfile.gettempdir(), uuid5(NAMESPACE_URL,str(randint(0, 1000000))).__str__().replace("-", ""))
os.mkdir(work_dir)


# config paths
config_file_name = 'config.ini'
path_to_config = os.path.join(work_dir, config_file_name)

# snapshot dir
snapshot_root = os.path.join(work_dir, "snapshots")
os.mkdir(snapshot_root)

# (optional) setting up block log
blockchain_dir = os.path.join(work_dir, "blockchain")
os.mkdir( blockchain_dir )
if args.block_log_path:
	os.symlink(args.block_log_path, os.path.join(blockchain_dir, "block_log"))
else:
	from hive_utils.common import get_testnet_block_log
	get_testnet_block_log(os.path.join(blockchain_dir, "block_log"))

# config
config = configuration()
config.witness = None	# no witness
config.private_key = None	# also no prv key
config.snapshot_root_dir = snapshot_root
config.plugin = config.plugin + " state_snapshot"	# this plugin is required

# config generation
config.generate(path_to_config)

base_hv_args = [ "--stop-replay-at-block", str(args.blocks), "--exit-after-replay" ]
hv_args = base_hv_args.copy()
hv_args.append("--replay-blockchain")

# setup for replay
node = hive_utils.hive_node.HiveNode(
	args.hived,
	work_dir, 
	hv_args.copy()
)

# replay
wait_for_node(node, "waiting for replay of {} blocks...".format(int(args.blocks)))
print("replay completed, creating snapshot")

# setup for first snapshot
first_snapshot_name = "first_snapshot"
hv_args = base_hv_args.copy()
hv_args.extend(["--dump-snapshot", first_snapshot_name])
node.hived_args = hv_args.copy()

# creating snapshot
wait_for_node(node, "creating snapshot '{}' ...".format(first_snapshot_name))

# setup for loading snapshot
hv_args = base_hv_args.copy()
hv_args.extend(["--load-snapshot", first_snapshot_name])
node.hived_args = hv_args.copy()
os.remove(os.path.join(blockchain_dir, "shared_memory.bin"))

# loading snapshot
wait_for_node(node, "creating snapshot '{}' ...".format(first_snapshot_name))

# setup for redumping snapshot
second_snapshot_name = "second_snapshot"
hv_args = base_hv_args.copy()
hv_args.extend(["--dump-snapshot", second_snapshot_name])
node.hived_args = hv_args.copy()

# redumpping snapshot
wait_for_node(node, "creating snapshot '{}' ...".format(second_snapshot_name))

path_to_first_snapshot = os.path.join(snapshot_root, first_snapshot_name)
path_to_second_snapshot = os.path.join(snapshot_root, second_snapshot_name)


from hive_utils.common import compare_snapshots
miss_list = compare_snapshots(path_to_first_snapshot, path_to_second_snapshot)

if len(miss_list) is not 0:
	for val in miss_list:
		print("checksum missmatch: '{}'".format(key))
else:
	print("all checksums matches")


if not args.leave:
	from shutil import rmtree
	rmtree( work_dir )
	print("deleted: {}".format(work_dir))
else:
	print("datadir not deleted: {}".format(work_dir))

exit(len(miss_list))
