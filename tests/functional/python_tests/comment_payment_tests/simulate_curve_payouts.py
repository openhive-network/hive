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
MAIN_LOG_PATH = "simulate_curve_payouts.log"
log_dir = os.environ.get("TEST_LOG_DIR", None)
if log_dir is not None:
    MAIN_LOG_PATH = log_dir + "/" + MAIN_LOG_PATH
else:
    MAIN_LOG_PATH = "./" + MAIN_LOG_PATH

MODULE_NAME = "Simulate-Curve-Payout-Tester"
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


def prepare_work_dir(work_dir_path, block_log_path, config_file_path):
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
        logger.info("Copying files [1/2]: block_log")
        copy(block_log_path, blockchain_dir + "/block_log")
        logger.info("Copying files [2/2]: config.ini")
        copy(config_file_path, work_dir_path + "/config.ini")


def perform_replay(node_bin_path, node_work_dir_path, is_steem_node):
    logger.info("Performing replay with {}".format(node_bin_path))
    node = hive_utils.hive_node.HiveNodeInScreen(node_bin_path, node_work_dir_path, None, True, is_steem_node)
    if node is not None:
        node.run_hive_node(["--enable-stale-production", "--replay-blockchain"])
    return node


def run_node(node_bin_path, node_work_dir_path, is_steem_node):
    logger.info("Running node with {}".format(node_bin_path))
    node = hive_utils.hive_node.HiveNodeInScreen(node_bin_path, node_work_dir_path, None, True, is_steem_node)
    if node is not None:
        node.run_hive_node(["--enable-stale-production"])
    return node


def get_data(node_url):
    curves = [
        "quadratic",
        "bounded_curation",
        "linear",
        "square_root",
        "convergent_linear",
        "convergent_square_root",
    ]

    ret = {}
    h = Hive(node_url)
    rf = h.rpc.get_reward_fund("post")
    ret["reward_fund"] = rf
    for curve in curves:
        ret[curve] = h.rpc.simulate_curve_payouts({"curve": curve, "var1": rf["content_constant"]}, api="rewards_api")

    return ret


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
        parser.add_argument(
            "--ref-node-is-steem",
            action="store_true",
            default=False,
            help="Set this flag if reference node is steem node",
        )
        parser.add_argument("--compare-as-text", action="store_true", default=False, help="Compare data as text")
        parser.add_argument("--no-replay", action="store_true", default=False, help="Do not perform data replay")
        args = parser.parse_args()

        if not args.no_replay:
            prepare_work_dir(args.reference_node_work_dir, args.block_log_path, "./resources/config.extended.ini")
            prepare_work_dir(args.test_node_work_dir, args.block_log_path, "./resources/config.extended.ini")

        logger.info("Reference node data")
        if args.no_replay:
            node = run_node(args.reference_node_bin_path, args.reference_node_work_dir, args.ref_node_is_steem)
        else:
            node = perform_replay(args.reference_node_bin_path, args.reference_node_work_dir, args.ref_node_is_steem)
        reference_data = get_data("http://127.0.0.1:8090")

        if node is not None:
            node.stop_hive_node()
            node = None

        logger.info("Test node data")
        if args.no_replay:
            node = run_node(args.test_node_bin_path, args.test_node_work_dir, False)
        else:
            node = perform_replay(args.test_node_bin_path, args.test_node_work_dir, False)
        test_data = get_data("http://127.0.0.1:8090")

        if node is not None:
            node.stop_hive_node()
            node = None

        from json import dumps

        if args.compare_as_text:
            for k, v in reference_data.items():
                logger.info("Comparing data under key: {}".format(k))
                v1 = dumps(v)
                v2 = dumps(test_data[k])
                if v1 != v2:
                    logger.error("Differences detected in data with key {}: {} != {}".format(k, v1, v2))
                    sys.exit(1)
        else:
            from jsondiff import diff

            for k, v in reference_data.items():
                logger.info("Comparing data under key: {}".format(k))
                json_diff = diff(v, test_data[k])
                if json_diff:
                    logger.error(
                        "Differences detected in data with key {}: {}".format(
                            k, dumps(json_diff, sort_keys=True, indent=4)
                        )
                    )
                    sys.exit(1)

    except Exception as ex:
        logger.exception("Exception: {}".format(ex))
        if node is not None:
            node.stop_hive_node()
        sys.exit(1)
