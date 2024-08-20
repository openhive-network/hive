from __future__ import annotations

import logging
import os
import sys

import hive_utils
import test_tools as tt

LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
MAIN_LOG_PATH = "comment_payment_001.log"
log_dir = os.environ.get("TEST_LOG_DIR", None)
MAIN_LOG_PATH = log_dir + "/" + MAIN_LOG_PATH if log_dir is not None else "./" + MAIN_LOG_PATH

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
    pass
except Exception:
    logger.error("beem library is not installed.")
    sys.exit(1)


def compare_files_by_line(file1, file2):
    try:
        line_cnt = 0
        with open(sys.argv[1]) as src, open(sys.argv[2]) as dst:
            for line in src:
                src_line = line.strip()
                dst_line = dst.readline().strip()
                assert src_line == dst_line, f"On line: {line_cnt} --> {src_line} != {dst_line}"
                line_cnt += 1
        logger.info(f"OK. Compared {line_cnt} lines.")
    except Exception as ex:
        logger.info(f"Exception: {ex}")
        return False
    return True


def prepare_work_dir(work_dir_path, block_log_path, config_file_path, overwrite=False):
    import os
    from shutil import copy, rmtree

    logger.info(f"Preparing working dir {work_dir_path}")
    blockchain_dir = work_dir_path + "/blockchain"
    if os.path.exists(work_dir_path):
        if os.path.isdir(work_dir_path):
            logger.info(f"Cleaning up working dir {work_dir_path}")
            paths_to_delete = [
                work_dir_path + "/p2p",
                work_dir_path + "/logs",
                blockchain_dir + "/shared_memory.bin",
            ]
            from glob import glob

            paths_to_delete.extend(glob(work_dir_path + "/*.log"))
            paths_to_delete.extend(glob(work_dir_path + "/*.pid"))
            paths_to_delete.extend(glob(work_dir_path + "/*.cfg"))
            paths_to_delete.extend(glob(blockchain_dir + "/*.artifacts"))
            for path in paths_to_delete:
                if os.path.exists(path):
                    logger.info(f"Removing {path}")
                    if os.path.isdir(path):
                        rmtree(path)
                    else:
                        os.remove(path)
        else:
            raise RuntimeError(f"{work_dir_path} is not a directory")
    else:
        logger.info(f"Creating directory {work_dir_path}")
        os.makedirs(work_dir_path)
        logger.info(f"Creating directory {blockchain_dir}")
        os.makedirs(blockchain_dir)
        logger.info("Copying files...")
        paths_to_copy = tt.BlockLog.get_existing_block_files(False, blockchain_dir)
        paths_to_copy.extend(tt.BlockLog.get_existing_block_files(True, blockchain_dir))
        for path in paths_to_copy:
            if os.path.exists(path):
                copy(block_log_path, path)
        copy(config_file_path, work_dir_path + "/config.ini")


def perform_replay(node_bin_path, node_work_dir_path):
    logger.info(f"Performing replay with {node_bin_path}")
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
    except Exception:
        logger.exception("Exception occurred")
        if node is not None:
            node.stop_hive_node()
        sys.exit(1)
