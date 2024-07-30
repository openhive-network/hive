from __future__ import annotations

import datetime
import logging
import os
import subprocess
import sys
from threading import Lock
from typing import Any, ClassVar

from .common import DEFAULT_LOG_FORMAT, DEFAULT_LOG_LEVEL

MODULE_NAME = "hive-utils.hive-node"

logger = logging.getLogger(MODULE_NAME)
logger.setLevel(DEFAULT_LOG_LEVEL)

ch = logging.StreamHandler(sys.stdout)
ch.setLevel(DEFAULT_LOG_LEVEL)
ch.setFormatter(logging.Formatter(DEFAULT_LOG_FORMAT))

logger.addHandler(ch)


class HiveNode:
    hived_binary = None
    hived_process = None
    hived_lock = Lock()
    hived_data_dir = None
    hived_args: ClassVar[list[Any]] = []

    def __init__(
        self, binary_path: str, working_dir: str, binary_args: list, stdout_stream=subprocess.PIPE, stderr_stream=None
    ):
        logger.info("New hive node")
        if not os.path.exists(binary_path):
            raise ValueError("Path to hived binary is not valid.")
        if not os.path.isfile(binary_path):
            raise ValueError("Path to hived binary must point to file")
        self.hived_binary = binary_path

        if not os.path.exists(working_dir):
            raise ValueError("Path to data directory is not valid")
        if not os.path.isdir(working_dir):
            raise ValueError("Data directory is not valid directory")
        self.hived_data_dir = working_dir

        if binary_args:
            self.hived_args.extend(binary_args)

        self.stdout_stream = stdout_stream
        self.stderr_stream = stderr_stream

    def __enter__(self):
        self.hived_lock.acquire()

        from subprocess import Popen
        from time import sleep

        hived_command = [self.hived_binary, f"--data-dir={self.hived_data_dir}"]
        hived_command.extend(self.hived_args)

        self.hived_process = Popen(hived_command, stdout=self.stdout_stream, stderr=self.stderr_stream)
        self.hived_process.poll()
        sleep(5)

        if self.hived_process.returncode:
            raise Exception("Error during starting node")  # noqa: TRY002

        self.last_returncode = None

    def get_output(self):
        out, err = self.hived_process.communicate()
        return out

    def __exit__(self, exc, value, tb):
        logger.info("Closing node")
        from signal import SIGINT, SIGTERM
        from time import sleep

        from psutil import pid_exists

        if self.hived_process is not None:
            self.hived_process.poll()
            if pid_exists(self.hived_process.pid):
                self.hived_process.send_signal(SIGINT)
                sleep(7)
                self.hived_process.poll()
                if pid_exists(self.hived_process.pid):
                    self.hived_process.send_signal(SIGTERM)
                    sleep(7)
                    self.hived_process.poll()
                    if pid_exists(self.hived_process.pid):
                        raise Exception("Error during stopping node. Manual intervention required.")  # noqa: TRY002
        self.last_returncode = self.hived_process.returncode
        self.hived_process = None
        self.hived_lock.release()

    # waits for node, to close. Recomended to use with `--exit-before-sync` flag
    def wait_till_end(self):
        assert self.hived_process is not None
        # assert "--exit-before-sync" in self.hived_args

        from time import sleep

        from psutil import STATUS_ZOMBIE, Process, pid_exists

        pid = self.hived_process.pid
        while pid_exists(pid) and Process(pid).status() != STATUS_ZOMBIE:
            sleep(0.25)


class HiveNodeInScreen:
    def __init__(
        self, hive_executable, working_dir, config_src_path, run_using_existing_data=False, node_is_steem=False
    ):
        self.hive_executable = hive_executable
        self.working_dir = working_dir
        self.config_src_path = config_src_path
        self.node_is_steem = node_is_steem

        # usefull when we want to do a replay
        if not run_using_existing_data:
            from shutil import copy, rmtree

            # remove old data from node
            if os.path.exists(self.working_dir):
                rmtree(self.working_dir)
            os.makedirs(self.working_dir + "/blockchain")
            # copy config file to working dir
            copy(self.config_src_path, self.working_dir + "/config.ini")

        self.hive_config = self.parse_node_config_file(self.working_dir + "/config.ini")
        self.ip_address, self.port = self.hive_config["webserver-http-endpoint"][0].split(":")
        self.ip_address = f"http://{self.ip_address}"
        self.node_running = False

    def get_from_config(self, key):
        return self.hive_config.get(key, None)

    def get_node_url(self):
        return f"{self.ip_address}:{self.port}/"

    def is_running(self):
        return self.node_running

    def parse_node_config_file(self, config_file_name):
        ret = {}
        lines = None
        with open(config_file_name) as f:
            lines = f.readlines()

        for line in lines:
            proc_line = line.strip()
            if proc_line:
                if proc_line.startswith("#"):
                    continue
                k, v = proc_line.split("=", 1)
                k = k.strip()
                v = v.strip()
                if k in ret:
                    ret[k].append(v)
                else:
                    ret[k] = [v]
        return ret

    def run_hive_node(self, additional_params=None, wait_for_blocks=True):
        from .common import (
            detect_process_by_name,
            kill_process,
            save_pid_file,
            save_screen_cfg,
            wait_for_string_in_file,
            wait_n_blocks,
        )

        if additional_params is None:
            additional_params = []
        if detect_process_by_name("hived" if not self.node_is_steem else "steemd", self.hive_executable, self.port):
            msg = "{} process is running on {}:{}. Please terminate that process and try again.".format(
                "hive", self.ip_address, self.port
            )
            raise ProcessLookupError(msg)

        logger.info(f"*** START NODE at {self.ip_address}:{self.port} in {self.working_dir}")

        parameters = [self.hive_executable, "-d", self.working_dir, "--advanced-benchmark"]

        parameters = parameters + additional_params

        self.pid_file_name = f"{self.working_dir}/run_hive-{self.port}.pid"
        current_time_str = datetime.datetime.now().strftime("%Y-%m-%d")
        log_file_name = f"{self.working_dir}/hive-{self.port}-{current_time_str}.log"
        screen_cfg_name = f"{self.working_dir}/hive_screen-{self.port}.cfg"

        save_screen_cfg(screen_cfg_name, log_file_name)
        screen_params = [
            "screen",
            "-m",
            "-d",
            "-L",
            "-c",
            screen_cfg_name,
            "-S",
            f"hive-{self.port}-{current_time_str}",
        ]

        parameters = screen_params + parameters
        logger.info(f"Running hived with command: {' '.join(parameters)}")

        try:
            subprocess.Popen(parameters)
            save_pid_file(self.pid_file_name, "hive", self.port, current_time_str)
            # we will allow for screen to setup and die maybe?
            from time import sleep

            sleep(5)
            # now it should be dead

            if not detect_process_by_name(
                "hived" if not self.node_is_steem else "steemd", self.hive_executable, self.port
            ):
                msg = "{} process is not running on {}:{}. Please check logs.".format(
                    "hive", self.ip_address, self.port
                )
                raise ProcessLookupError(msg)  # noqa: TRY301

            if "--replay-blockchain" in parameters:
                wait_for_string_in_file(log_file_name, "start listening for ws requests", None)
            else:
                if wait_for_blocks:
                    wait_n_blocks(f"{self.ip_address}:{self.port}", 5)
                else:
                    wait_for_string_in_file(log_file_name, "start listening for ws requests", 20.0)
            self.node_running = True
            logger.info(f"Node at {self.ip_address}:{self.port} in {self.working_dir} is up and running...")
        except Exception as ex:
            logger.error(f"Exception during hived run: {ex}")
            kill_process(self.pid_file_name, "hive" if not self.node_is_steem else "steem", self.ip_address, self.port)
            self.node_running = False

    def stop_hive_node(self):
        from .common import kill_process

        logger.info(f"Stopping node at {self.ip_address}:{self.port}")
        kill_process(self.pid_file_name, "hive" if not self.node_is_steem else "steem", self.ip_address, self.port)
        self.node_running = False

    def __enter__(self):
        self.run_hive_node()

    def __exit__(self, exc, value, tb):
        self.stop_hive_node()


if __name__ == "__main__":
    KEEP_GOING = True

    def sigint_handler(signum, frame):
        logger.info("Shutting down...")
        global KEEP_GOING
        from time import sleep

        KEEP_GOING = False
        sleep(3)
        sys.exit(0)

    def main():
        try:
            import signal

            signal.signal(signal.SIGINT, sigint_handler)

            plugins = ["chain", "p2p", "webserver", "json_rpc", "debug_node"]
            config = (
                "# Simple config file\n"
                "shared-file-size = 1G\n"
                "enable-stale-production = true\n"
                "p2p-endpoint = 127.0.0.1:2001\n"
                "webserver-http-endpoint = 127.0.0.1:8095\n"
                "webserver-ws-endpoint = 127.0.0.1:8096\n"
                f"plugin = witness debug_node {' '.join(plugins)}\n"
                "plugin = database_api debug_node_api block_api\n"
                'witness = "initminer"\n'
                "private-key = 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n\n"
                "required-participation = 0"
            )

            binary_path = "/home/dariusz-work/Builds/hive/programs/hived/hived"
            work_dir = "/home/dariusz-work/hive-data"

            print(config)

            with open(work_dir + "/config.ini", "w") as conf_file:
                conf_file.write(config)

            node = HiveNode(binary_path, work_dir, [])
            from time import sleep

            from .common import debug_generate_blocks, wait_n_blocks

            with node:
                print("Waiting 10 blocks")
                wait_n_blocks("http://127.0.0.1:8095", 10)
                print("Done...")
                print(
                    debug_generate_blocks(
                        "http://127.0.0.1:8095", "5JHNbFNDg834SFj8CMArV6YW7td4zrPzXveqTfaShmYVuYNeK69", 100
                    )
                )
                while KEEP_GOING:
                    sleep(1)
        except Exception:
            logger.exception("Exception occurred")
            sys.exit(1)

    main()
