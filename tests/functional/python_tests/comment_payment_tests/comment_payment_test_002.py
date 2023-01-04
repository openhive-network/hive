#!/usr/bin/python3

import sys

sys.path.append("../../")
import hive_utils

from uuid import uuid4
from time import sleep
import logging
import test_utils
import os


LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
MAIN_LOG_PATH = "comment_payment_001.log"
log_dir = os.environ.get("TEST_LOG_DIR", None)
if log_dir is not None:
    MAIN_LOG_PATH = log_dir + "/" + MAIN_LOG_PATH
else:
    MAIN_LOG_PATH = "./" + MAIN_LOG_PATH

MODULE_NAME = "Comment-Payment-Tester"
logger = logging.getLogger(MODULE_NAME)
logger.setLevel(LOG_LEVEL)

ch = logging.StreamHandler(sys.stdout)
ch.setLevel(LOG_LEVEL)
ch.setFormatter(logging.Formatter(LOG_FORMAT))

fh = logging.FileHandler(MAIN_LOG_PATH)
fh.setLevel(LOG_LEVEL)
fh.setFormatter(logging.Formatter(LOG_FORMAT))

if not logger.hasHandlers():
    logger.addHandler(ch)
    logger.addHandler(fh)

try:
    from beem import Hive
except Exception as ex:
    logger.error("beem library is not installed.")
    sys.exit(1)


def compare_files_by_line(file1, file2):
    try:
        line_cnt = 0
        with open(sys.argv[1], "r") as src:
            with open(sys.argv[2], "r") as dst:
                for line in src:
                    src_line = line.strip()
                    dst_line = dst.readline().strip()
                    assert src_line == dst_line, "On line: {} --> {} != {}".format(line_cnt, src_line, dst_line)
                    line_cnt += 1
        logger.info("OK. Compared {} lines.".format(line_cnt))
        return True
    except Exception as ex:
        logger.info("Exception: {}".format(ex))
        return False


def prepare_work_dir(work_dir_path, block_log_path, config_file_path, overwrite=False):
    import os
    from shutil import copy, rmtree

    logger.info("Preparing working dir {}".format(work_dir_path))
    blockchain_dir = work_dir_path + "/blockchain"
    if os.path.exists(work_dir_path):
        if os.path.isdir(work_dir_path):
            logger.info("Cleaning up working dir {}".format(work_dir_path))
            paths_to_delete = [
                work_dir_path + "/p2p",
                work_dir_path + "/logs",
                blockchain_dir + "/block_log.index",
                blockchain_dir + "/shared_memory.bin",
            ]
            from glob import glob

            paths_to_delete.extend(glob(work_dir_path + "/*.log"))
            paths_to_delete.extend(glob(work_dir_path + "/*.pid"))
            paths_to_delete.extend(glob(work_dir_path + "/*.cfg"))
            for path in paths_to_delete:
                if os.path.exists(path):
                    logger.info("Removing {}".format(path))
                    if os.path.isdir(path):
                        rmtree(path)
                    else:
                        os.remove(path)
        else:
            raise RuntimeError("{} is not a directory".format(work_dir_path))
    else:
        logger.info("Creating directory {}".format(work_dir_path))
        os.makedirs(work_dir_path)
        logger.info("Creating directory {}".format(blockchain_dir))
        os.makedirs(blockchain_dir)
        logger.info("Copying files...")
        copy(block_log_path, blockchain_dir + "/block_log")
        copy(config_file_path, work_dir_path + "/config.ini")


def perform_replay(node_bin_path, node_work_dir_path):
    logger.info("Performing replay with {}".format(node_bin_path))
    node = hive_utils.hive_node.HiveNodeInScreen(node_bin_path, node_work_dir_path, None, True)
    if node is not None:
        node.run_hive_node(
            ["--enable-stale-production", "--replay-blockchain", "--cashout-logging-log-path-dir", node_work_dir_path]
        )
    return node


if __name__ == "__main__":
    node = None
    try:
        logger.info("Performing comment payment tests")
        import argparse

        parser = argparse.ArgumentParser()
        parser.add_argument("reference_node_bin_path", help="Path to reference hived executable.")
        parser.add_argument("test_node_bin_path", help="Path to test hived executable.")
        parser.add_argument("reference_node_work_dir", help="Path to reference node working dir.")
        parser.add_argument("test_node_work_dir", help="Path to reference node working dir.")
        parser.add_argument("block_log_path", help="Path to block log for replay.")
        parser.add_argument("--replay-reference-node", type=bool, default=True, help="Replay reference node.")
        args = parser.parse_args()

        if args.replay_reference_node:
            prepare_work_dir(args.reference_node_work_dir, args.block_log_path, "./resources/config.ini")

        prepare_work_dir(args.test_node_work_dir, args.block_log_path, "./resources/config.ini")

        if args.replay_reference_node:
            node = perform_replay(args.reference_node_bin_path, args.reference_node_work_dir)
            if node is not None:
                node.stop_hive_node()
                node = None

        node = perform_replay(args.test_node_bin_path, args.test_node_work_dir)
        if node is not None:
            node.stop_hive_node()
            node = None

        logger.info("Comparing comment cashout logs...")
        from glob import glob

        files_to_compare = glob(args.reference_node_work_dir + "/*-*.log")
        for f in files_to_compare:
            file_name = f.split("/")[-1]
            if not compare_files_by_line(f, args.reference_node_work_dir + "/" + file_name):
                sys.exit(1)
        sys.exit(0)
    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        if node is not None:
            node.stop_hive_node()
        sys.exit(1)
