#!/usr/bin/python3

import os
import argparse
import time
import subprocess
import random
import os.path
from subprocess import check_output
from datetime import datetime

cnt = 0


def try_generate_crash(command_line, dump_file_str, crash_time, wait_time, exec_name):

    global cnt

    with open(dump_file_str, "w") as dump_file:
        subprocess.Popen(command_line, shell=True, stdout=dump_file, stderr=subprocess.STDOUT)

        time.sleep(crash_time)

        pid = ""
        try:
            pid = check_output(["pidof", exec_name], stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as e:
            print("An error occured during PID retrieving {}. Maybe replaying is finished completely".format(e.output))
            return False

        pid = str(pid)
        pid = pid[2 : len(pid) - 3]

        cnt += 1
        print("cnt: {} wait-time: {}s pid: {}".format(cnt, crash_time, pid))

        command = "kill -2 " + str(pid)
        os.system(command)

        time.sleep(wait_time)

    with open(dump_file_str, "r") as f:
        for line in f:
            pass
        last_line = line

    found = last_line.find("exited cleanly")

    if found == -1:
        print("Incorrect closure!!!: `{}`".format(last_line))

    return found != -1


if __name__ == "__main__":

    import argparse

    parser = argparse.ArgumentParser()

    # Example
    # ./test-sigint-close.py --path "/home/mario/src/01.HIVE-HIVE/hive/build_release_main/programs/hived/hived" --dir "/home/mario/src/hive-data/" --block_limit 5000000 --wait_time 5
    parser.add_argument("--path", help="Path to hived executable.", default="")
    parser.add_argument("--dir", help="Path to directory with blockchain data.", default="")
    parser.add_argument("--block_limit", help="Maximum number of blocks to be processed.", default=0)
    parser.add_argument("--wait_time", help="Delay before sending SIGINT and after closing a node [s]", default=5)

    args = parser.parse_args()

    dump_file_str = "crash.log"

    exec_name = ""
    parts = args.path.split("/")
    if len(parts) == 1:
        exec_name = parts[0][2:]
    else:
        exec_name = parts[len(parts) - 1]

    command_line = (
        args.path
        + " -d "
        + args.dir
        + " --replay-blockchain --exit-after-replay --stop-replay-at-block "
        + str(args.block_limit)
    )

    # how long to wait after closing[ s ]
    wait_time = int(args.wait_time)
    milliseconds_limit = 2000
    res = True
    while res:

        # how long to wait before closing[ ms ] - minimum `wait_time` + random( 0-2s ) because p2p plugin must start correctly
        crash_time = random.randint(1, milliseconds_limit) / 1000
        crash_time += wait_time

        res = try_generate_crash(command_line, dump_file_str, crash_time, wait_time, exec_name)
