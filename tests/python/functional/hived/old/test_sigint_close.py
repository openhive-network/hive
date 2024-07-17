from __future__ import annotations

import argparse
import os
import os.path
import random
import subprocess
import time
from subprocess import check_output

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
            print(f"An error occured during PID retrieving {e.output}. Maybe replaying is finished completely")
            return False

        pid = str(pid)
        pid = pid[2 : len(pid) - 3]

        cnt += 1
        print(f"cnt: {cnt} wait-time: {crash_time}s pid: {pid}")

        command = "kill -2 " + str(pid)
        os.system(command)

        time.sleep(wait_time)

    with open(dump_file_str) as f:
        for line in f:  # noqa: B007
            pass
        last_line = line

    found = last_line.find("exited cleanly")

    if found == -1:
        print(f"Incorrect closure!!!: `{last_line}`")

    return found != -1


if __name__ == "__main__":
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
    exec_name = parts[0][2:] if len(parts) == 1 else parts[len(parts) - 1]

    command_line = (
        args.path
        + " -d "
        + args.dir
        + " --replay-blockchain --exit-after-replay --stop-at-block "
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
