#!/usr/bin/python3

import sys
import os
import tempfile
import argparse
from subprocess import PIPE, STDOUT

sys.path.append("../../")

import hive_utils
from hive_utils.resources.configini import config as configuration
from hive_utils.common import wait_n_blocks


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
# os.mkdir(snapshot_root)

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

def get_base_hv_args():
	return [ "--stop-replay-at-block", str(args.blocks), "--exit-after-replay" ].copy()

def dump_snapshot(Node, snapshot_name):
# setup for snapshot
	hv_args = get_base_hv_args()
	hv_args.extend(["--dump-snapshot", snapshot_name])
	Node.hived_args = hv_args
# creating snapshot
	print("creating snapshot '{}' ...".format(snapshot_name))
	with Node:
		Node.wait_till_end()

def load_snapshot(Node, snapshot_name):
# setup for loading snapshot
	hv_args = get_base_hv_args()
	hv_args.extend(["--load-snapshot", snapshot_name])
	Node.hived_args = hv_args
	os.remove(os.path.join(blockchain_dir, "shared_memory.bin"))
# loading snapshot
	print( "loading snapshot '{}' ...".format(snapshot_name))
	with Node:
		Node.wait_till_end()

def run_for_n_blocks(Node, blocks : int, additional_args : list = []):
	args.blocks += blocks
	Node.hived_args = get_base_hv_args()
	if len(additional_args) > 0:
		Node.hived_args.extend(additional_args)
	print("waiting for {} blocks...".format(int(args.blocks)))
	with Node:
		Node.wait_till_end()

def require_success(node):
	assert node.last_returncode == 0

def require_fail(node, filepath):
	with open(filepath, 'r') as fff:
		for line in fff:
			if line.find("Snapshot generation FAILED."):
				return
	assert False

hv_args = get_base_hv_args()
hv_args.append("--replay-blockchain")

# setting up logging
stdout = PIPE
stderr = None

if args.artifacts:
	stderr = STDOUT
	stdout = open(os.path.join(args.artifacts, "replayed_node_snapshot_2.log"), 'w', 1)

# setup for replay
node = hive_utils.hive_node.HiveNode(
	args.hived,
	work_dir, 
	hv_args,
	stdout,
	stderr
)

# replay
print("waiting for replay of {} blocks...".format(int(args.blocks)))
with node:
	node.wait_till_end()
require_success(node)
print("replay completed, creating snapshot")


# try to create snapshot, while directory not exist
assert not os.path.exists(snapshot_root)
dump_snapshot(node, "snap_1")
require_success(node)
assert os.path.exists(snapshot_root)
assert os.path.exists(os.path.join(snapshot_root, "snap_1"))

# load snapshot
load_snapshot(node, "snap_1")
require_success(node)

# check is replaying 
run_for_n_blocks(node, 100, ["--replay-blockchain"])
require_success(node)

# dump to same directory
# change output stream
tmp_filel = "tmp_file.log"
tmp_output = open(tmp_filel, 'w')
current_outuput = node.stdout_stream
node.stdout_stream = tmp_output

# gather output
dump_snapshot(node, "snap_1")

# optionally append output to artiffacts
tmp_output.close()
node.stdout_stream = current_outuput
if stderr is not None:
	with open(tmp_filel, 'r'):
		for line in tmp_filel:
			stdout.write(line)

# check is failed
require_fail(node, tmp_filel)
os.remove(tmp_filel)

from shutil import rmtree
rmtree(os.path.join(config.snapshot_root_dir, "snap_1"))
dump_snapshot(node, "snap_1")
require_success(node)

# load snapshot
load_snapshot(node, "snap_1")
require_success(node)

# check is replaying 
run_for_n_blocks(node, 100, ["--replay-blockchain"])
require_success(node)

print("success")

if not args.leave:
	from shutil import rmtree
	rmtree( work_dir )
	print("deleted: {}".format(work_dir))
else:
	print("datadir not deleted: {}".format(work_dir))

if stderr is not None:
	stdout.close()

exit(0)