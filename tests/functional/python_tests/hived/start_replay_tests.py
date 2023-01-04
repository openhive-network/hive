#!/usr/bin/python3

import argparse
import os
import subprocess as sub
import sys
import tempfile

sys.path.append("../../")

import hive_utils
from hive_utils.hive_node import HiveNode
from hive_utils.resources.configini import config as configuration

parser = argparse.ArgumentParser()
parser.add_argument("--run-hived", dest="hived", help="Path to hived executable", required=True, type=str)
parser.add_argument(
    "--block-log", dest="block_log_path", help="Path to block log", required=True, type=str, default=None
)
parser.add_argument("--test-directory", dest="test_dir", help="Path to directory with tests", required=True, type=str)
parser.add_argument("--blocks", dest="blocks", help="Blocks to replay", required=False, type=int, default=5000000)
parser.add_argument(
    "--artifact-directory",
    dest="artifacts",
    help="Path to directory where logs will be stored",
    required=False,
    type=str,
)
parser.add_argument("--leave", dest="leave", action="store_true")

args = parser.parse_args()

# check is blocks to replay are in range
assert args.blocks <= 5000000, "cannot replay more than 5 million blocks"
if args.blocks < 1000000:
    from warnings import warn

    warn("replay under 1 million blocks can be unreliable")

# config setup
config = configuration()
config.witness = None
config.private_key = None
config.webserver_http_endpoint = "127.0.0.1:8100"
config.exclude_plugins(["witness"])
config.update_plugins(
    [
        "account_by_key",
        "account_by_key_api",
        "condenser_api",
        "account_history_rocksdb",
        "account_history_api",
        "block_api",
        "chain_api",
    ]
)

from random import randint

# creating working dir
from uuid import NAMESPACE_URL, uuid5

work_dir = os.path.join(
    tempfile.gettempdir(), uuid5(NAMESPACE_URL, str(randint(0, 1000000))).__str__().replace("-", "")
)
os.mkdir(work_dir)

# setting up block log
assert args.block_log_path
blockchain_dir = os.path.join(work_dir, "blockchain")

os.mkdir(blockchain_dir)
os.symlink(args.block_log_path, os.path.join(blockchain_dir, "block_log"))

# generating config
path_to_config = os.path.join(work_dir, "config.ini")
config.generate(path_to_config)

# setting up logging
stdout = sub.PIPE
stderr = None

if args.artifacts:
    stderr = sub.STDOUT
    stdout = open(os.path.join(args.artifacts, "replayed_node.log"), "w", 1)

# creating node
node = HiveNode(
    args.hived,
    work_dir,
    ["--stop-replay-at-block", str(args.blocks), "--replay-blockchain", "--exit-after-replay"],
    stdout,
    stderr,
)

# replay
with node:
    node.wait_till_end()

node.hived_args = ["--stop-replay-at-block", str(args.blocks), "--replay-blockchain"]

# check success
assert node.last_returncode == 0

# gather aviable tests
os.chdir(args.test_dir)
from os import walk

(_, _, filenames) = next(walk(args.test_dir))
tests = [x for x in filenames if x.endswith(".py")]

# tests
PID: int = None
RETCODE: int = 0
MAX_TIME_FOR_EACH_TEST_IN_SECONDS = 15 * 60

if args.artifacts:
    if not os.path.exists(args.artifacts):
        os.mkdir(args.artifacts)

try:
    with node:

        # gather required info[]
        PID = node.hived_process.pid

        # start tests
        for test in tests:

            output = sub.DEVNULL

            if args.artifacts:
                log_filename = test.split(".")[0] + ".log"
                path_to_save = os.path.join(args.artifacts, log_filename)
                output = open(path_to_save, "w", 1)

            T = sub.Popen(
                [
                    "/usr/bin/python3",
                    test,
                    "--run-hived",
                    config.webserver_http_endpoint,
                    "--path-to-config",
                    path_to_config,
                ],
                stdout=output,
                stderr=sub.STDOUT,
            )
            retcode = T.wait(MAX_TIME_FOR_EACH_TEST_IN_SECONDS)
            if retcode != 0:
                RETCODE -= 1
                print(f"[FAILED] {test}")
            else:
                print(f"[OK] {test}")

            if output is not sub.DEVNULL:
                # output.flush()
                output.close()


except Exception as exception:

    RETCODE -= 100
    print(f"Exception occured:\n####\n\n{exception}\n\n####")

finally:

    from psutil import Process, pid_exists

    while pid_exists(PID):
        proc = Process(PID)
        timeout = 5
        try:
            print(f"[{timeout} seconds] Trying to kill process PID: {PID}")
            proc.kill()
            proc.wait(timeout)
        except:
            pass
        finally:
            if pid_exists(PID):
                try:
                    print(f"[{timeout} seconds] Trying to kill process PID: {PID}")
                    proc.terminate()
                    proc.wait(timeout)
                except:
                    print(f"Process is out of controll PID: {PID}")

    if stderr is not None:
        stdout.close()

    if not args.leave:
        from shutil import rmtree

        rmtree(work_dir)
        print("deleted: {}".format(work_dir))
    else:
        print("datadir not deleted: {}".format(work_dir))

    exit(RETCODE)
