#!/usr/bin/python3

import sys
import os
import tempfile
import argparse

sys.path.append("../../")

import hive_utils
from hive_utils.resources.configini import config as configuration


parser = argparse.ArgumentParser()
parser.add_argument("--hived-path", dest="hived", help = "Path to hived executable", required=True, type=str)
parser.add_argument("--block-log", dest="block_log_path", help = "Path to block log", required=False, type=str, default=None)
parser.add_argument("--blocks", dest="blocks", help = "Blocks to replay", required=False, type=int, default=100000)
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
if args.block_log_path:
	blockchain_dir = os.path.join(work_dir, "blockchain")
	os.mkdir( blockchain_dir )
	os.symlink(args.block_log_path, os.path.join(blockchain_dir, "block_log"))

# config
config = configuration()
config.witness = None	# no witness
config.private_key = None	# also no prv key
config.snapshot_root_dir = snapshot_root
config.plugin = config.plugin + " state_snapshot"	# this plugin is required

# config generation
config.generate(path_to_config)

def wait_for_node(node_to_wait, exit_msg, msg):
	from time import sleep
	print(msg)
	with node_to_wait:
		ret = node_to_wait.get_output().decode('utf-8')
		while not ret.find(exit_msg):
			sleep(0.25)
			ret = node_to_wait.get_output().decode('utf-8')

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
wait_for_node(node, "exited cleanly", "waiting for replay of {} blocks...".format(int(args.blocks)))
print("replay completed, creating snapshot")

# setup for first snapshot
first_snapshot_name = "first_snapshot"
hv_args = base_hv_args.copy()
hv_args.extend(["--dump-snapshot", first_snapshot_name])
node.hived_args = hv_args.copy()

# creating snapshot
wait_for_node(node, "Snapshot generation finished", "creating snapshot '{}' ...".format(first_snapshot_name))

# setup for loading snapshot
hv_args = base_hv_args.copy()
hv_args.extend(["--load-snapshot", first_snapshot_name])
node.hived_args = hv_args.copy()

# loading snapshot
wait_for_node(node, "Validate_invariants finished", "creating snapshot '{}' ...".format(first_snapshot_name))

# setup for redumping snapshot
second_snapshot_name = "second_snapshot"
hv_args = base_hv_args.copy()
hv_args.extend(["--dump-snapshot", second_snapshot_name])
node.hived_args = hv_args.copy()

# redumpping snapshot
wait_for_node(node, "Snapshot generation finished", "creating snapshot '{}' ...".format(second_snapshot_name))

# calculating md5 checsums for all files
def get_index_checksums(path : str):
	from subprocess import run, PIPE

	# gather files and paths for md5sum
	files = run(["find", path_to_first_snapshot, "-type", "f" ], stdout=PIPE).stdout.decode('utf-8').splitlines()
	temp_files = []
	for file in files:
		if file.find('snapshot-manifest') < 0:
			temp_files.append(file)
	files = temp_files
	
	# calculate md5sum
	files_with_md5 = dict()
	for file in files:
		md5, _, output = run(["md5sum", file ], stdout=PIPE).stdout.decode('utf-8').strip().split(' ')
		output = os.path.split(os.path.split(output)[0])[1]
		files_with_md5[output] = md5

	return files_with_md5

path_to_first_snapshot = os.path.join(snapshot_root, first_snapshot_name)
path_to_second_snapshot = os.path.join(snapshot_root, second_snapshot_name)

first_ret = get_index_checksums(path_to_first_snapshot)
second_ret = get_index_checksums(path_to_second_snapshot)

keys = first_ret.keys()
ret_code = 0
for key in keys:
	first_md5 = first_ret[key]
	second_md5 = second_ret[key]
	if first_md5 != second_md5:
		print("Checksum Missmatch: '{}'".format(key))
		ret_code += 1
	else:
		print("Checksum Match: '{}'".format(key))

if not args.leave:
	from shutil import rmtree
	rmtree( work_dir )
	print("Deleted: {}".format(work_dir))
else:
	print("Datadir not deleted: {}".format(work_dir))

exit(ret_code)
