#!/usr/bin/python3

import sys
import os
import tempfile
import argparse

sys.path.append("../../")

import hive_utils
from hive_utils.resources.configini import config as configuration
from subprocess import PIPE, STDOUT


parser = argparse.ArgumentParser()
parser.add_argument("--run-hived", dest="hived", help = "Path to hived executable", required=True, type=str)
parser.add_argument("--block-log", dest="block_log_path", help = "Path to block log", required=True, type=str, default=None)
parser.add_argument("--blocks", dest="blocks", help = "Blocks to replay", required=False, type=int, default=1000)
parser.add_argument("--leave", dest="leave", action='store_true')
parser.add_argument("--artifact-directory", dest="artifacts", help = "Path to directory where logs will be stored", required=False, type=str)

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

# setting up block log
blockchain_dir = os.path.join(work_dir, "blockchain")
os.mkdir( blockchain_dir )
assert args.block_log_path
os.symlink(args.block_log_path, os.path.join(blockchain_dir, "block_log"))

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

# setting up logging
stdout = PIPE
stderr = None

if args.artifacts:
	stderr = STDOUT
	stdout = open(os.path.join(args.artifacts, "replayed_node_snapshot_1.log"), 'w', 1)

# setup for replay
node = hive_utils.hive_node.HiveNode(
	args.hived,
	work_dir, 
	hv_args.copy(),
	stdout,
	stderr
)

# replay
print(f"waiting for replay of {args.blocks} blocks...")
with node:
	node.wait_till_end()
print("replay completed, creating snapshot")

# setup for first snapshot
first_snapshot_name = "first_snapshot"
hv_args = base_hv_args.copy()
hv_args.extend(["--dump-snapshot", first_snapshot_name])
node.hived_args = hv_args.copy()

# creating snapshot
print("creating snapshot '{}' ...".format(first_snapshot_name))
with node:
	node.wait_till_end()

# setup for loading snapshot
hv_args = base_hv_args.copy()
hv_args.extend(["--load-snapshot", first_snapshot_name])
node.hived_args = hv_args.copy()
os.remove(os.path.join(blockchain_dir, "shared_memory.bin"))

# loading snapshot
print("creating snapshot '{}' ...".format(first_snapshot_name))
with node:
	node.wait_till_end()

# setup for redumping snapshot
second_snapshot_name = "second_snapshot"
hv_args = base_hv_args.copy()
hv_args.extend(["--dump-snapshot", second_snapshot_name])
node.hived_args = hv_args.copy()

# redumpping snapshot
print("creating snapshot '{}' ...".format(second_snapshot_name))
with node:
	node.wait_till_end()

path_to_first_snapshot = os.path.join(snapshot_root, first_snapshot_name)
path_to_second_snapshot = os.path.join(snapshot_root, second_snapshot_name)


from hive_utils.common import compare_snapshots
miss_list = compare_snapshots(path_to_first_snapshot, path_to_second_snapshot)

if len(miss_list) is not 0:
	for val in miss_list:
		print("checksum missmatch: '{}'".format(val))
else:
	print("all checksums matches")


if not args.leave:
	from shutil import rmtree
	rmtree( work_dir )
	print("deleted: {}".format(work_dir))
else:
	print("datadir not deleted: {}".format(work_dir))

if stderr is not None:
	stdout.close()

exit(len(miss_list))
