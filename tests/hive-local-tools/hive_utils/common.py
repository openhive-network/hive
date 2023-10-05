# -*- coding: utf-8 -*-

from __future__ import annotations

import logging
from junit_xml import TestCase
import time
from typing import TYPE_CHECKING
import sys
import traceback

from test_tools.__private.communication import CustomJsonEncoder

if TYPE_CHECKING:
    from typing import Union
    import test_tools as tt


DEFAULT_LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
DEFAULT_LOG_LEVEL = logging.INFO

MODULE_NAME = "hive-utils.common"
logger = logging.getLogger(MODULE_NAME)
logger.setLevel(DEFAULT_LOG_LEVEL)


def send_rpc_query(target_node: str, payload: dict) -> dict:
    from requests import post
    from json import dumps

    resp = post(target_node, data=dumps(payload, cls=CustomJsonEncoder))
    if resp.status_code != 200:
        print(resp.json())
        raise Exception("{} returned non 200 code".format(payload["method"]))
    return resp.json()


def get_random_id() -> str:
    from uuid import uuid4

    return str(uuid4())


def get_current_block_number(source_node) -> int:
    payload = {
        "jsonrpc": "2.0",
        "id": get_random_id(),
        "method": "database_api.get_dynamic_global_properties",
        "params": {},
    }

    from requests import post
    from json import dumps, loads

    try:
        resp = post(source_node, data=dumps(payload, cls=CustomJsonEncoder))
        if resp.status_code != 200:
            return -1
        data = resp.json()["result"]
        return int(data["head_block_number"])
    except Exception as ex:
        return -1


def wait_n_blocks(source_node: str, blocks: int, timeout: int = 60):
    from time import sleep

    starting_block = get_current_block_number(source_node)
    cntr = 0
    while starting_block == -1 and cntr < timeout:
        starting_block = get_current_block_number(source_node)
        sleep(1)
        cntr += 1
    if cntr >= timeout:
        raise TimeoutError("Timeout in waiting for blocks. Hived is not running?")
    current_block = starting_block
    cntr = 0
    while current_block - starting_block < blocks and cntr < timeout:
        current_block = get_current_block_number(source_node)
        sleep(1)
        cntr += 1
    if cntr >= timeout:
        raise TimeoutError("Timeout in waiting for blocks. Hived is not running?")


def debug_generate_blocks(target_node: str, debug_key: Union[str, tt.PrivateKey], count: int) -> dict:
    if count < 0:
        raise ValueError("Count must be a positive non-zero number")
    payload = {
        "jsonrpc": "2.0",
        "id": get_random_id(),
        "method": "debug_node_api.debug_generate_blocks",
        "params": {"debug_key": debug_key, "count": count, "skip": 0, "miss_blocks": 0, "edit_if_needed": True},
    }
    return send_rpc_query(target_node, payload)


def debug_generate_blocks_until(
    target_node: str, debug_key: Union[str, tt.PrivateKey], timestamp: str, generate_sparsely: bool = True
) -> dict:
    payload = {
        "jsonrpc": "2.0",
        "id": get_random_id(),
        "method": "debug_node_api.debug_generate_blocks_until",
        "params": {"debug_key": debug_key, "head_block_time": timestamp, "generate_sparsely": generate_sparsely},
    }
    return send_rpc_query(target_node, payload)


# safe_block_offset - how many blocks at the end, process with 'slower' option
def debug_quick_block_skip(node, debug_key, blocks, safe_block_offset: int = 100) -> None:
    assert blocks > safe_block_offset

    import datetime
    import dateutil.parser

    now = node.get_dynamic_global_properties(False).get("time", None)
    now = dateutil.parser.parse(now)
    now = now + datetime.timedelta(seconds=(blocks * 3) - safe_block_offset)

    debug_generate_blocks_until(node.rpc.url, debug_key, now.replace(microsecond=0).isoformat())
    debug_generate_blocks(node.rpc.url, debug_key, safe_block_offset)


def debug_set_hardfork(target_node: str, hardfork_id: int) -> dict:
    if hardfork_id < 0:
        raise ValueError("hardfork_id cannot be negative")
    payload = {
        "jsonrpc": "2.0",
        "id": get_random_id(),
        "method": "debug_node_api.debug_set_hardfork",
        "params": {"hardfork_id": hardfork_id},
    }
    return send_rpc_query(target_node, payload)


def debug_quick_block_skip_with_step(
    node, debug_key: Union[str, tt.PrivateKey], blocks, block_step, safe_block_offset: int = 100
) -> None:
    currently_processed = blocks
    while currently_processed > 0:
        if currently_processed - block_step <= 0:
            if safe_block_offset > currently_processed:
                debug_generate_blocks(node.rpc.url, debug_key, currently_processed)
            else:
                debug_quick_block_skip(node, debug_key, currently_processed, safe_block_offset)
            return
        else:
            debug_quick_block_skip(node, debug_key, block_step, safe_block_offset)
        currently_processed -= block_step


def debug_has_hardfork(target_node: str, hardfork_id: int) -> dict:
    payload = {
        "jsonrpc": "2.0",
        "id": get_random_id(),
        "method": "debug_node_api.debug_has_hardfork",
        "params": {"hardfork_id": hardfork_id},
    }
    return send_rpc_query(target_node, payload)


def debug_get_witness_schedule(target_node: str) -> dict:
    payload = {
        "jsonrpc": "2.0",
        "id": get_random_id(),
        "method": "debug_node_api.debug_get_witness_schedule",
        "params": {},
    }
    return send_rpc_query(target_node, payload)


def debug_get_future_witness_schedule(target_node: str) -> dict:
    payload = {
        "jsonrpc": "2.0",
        "id": get_random_id(),
        "method": "debug_node_api.debug_get_future_witness_schedule",
        "params": {},
    }
    return send_rpc_query(target_node, payload)


def debug_get_hardfork_property_object(target_node: str) -> dict:
    payload = {
        "jsonrpc": "2.0",
        "id": get_random_id(),
        "method": "debug_node_api.debug_get_hardfork_property_object",
        "params": {},
    }
    return send_rpc_query(target_node, payload)


def get_date_as_isostr(date):
    return date.replace(microsecond=0).isoformat()


def get_isostr_start_end_date(now, start_date_delta, end_date_delta):
    from datetime import timedelta

    start_date = now + timedelta(days=start_date_delta)
    end_date = start_date + timedelta(days=end_date_delta)

    start_date = start_date.replace(microsecond=0).isoformat()
    end_date = end_date.replace(microsecond=0).isoformat()

    return start_date, end_date


def save_screen_cfg(cfg_file_name, log_file_path):
    """Creates a config file for screen command. In config file we configure logging path and interval.

    Args:
        cfg_file_name -- file name for screen config file,
        log_file_path -- path to log file.
    """
    with open(cfg_file_name, "w") as cfg:
        cfg.write("logfile {0}\n".format(log_file_path))
        cfg.write("deflog on\n")
        cfg.write("logfile flush 1\n")


def save_pid_file(pid_file_name, exec_name, port, start_time):
    """Creates PID file which indicate running keosd or nodeos process.

    Args:
        pid_file_name -- file name for pid file,
        exec_name -- name of the exectutable bound to this pid file,
        port -- port number for running executable,
        start_time -- execution start time.
    """
    with open(pid_file_name, "w") as pid_file:
        pid_file.write("{0}-{1}-{2}\n".format(exec_name, port, start_time))


def wait_for_string_in_file(log_file_name, string, timeout):
    """Blocks program execution until a given string is found in given file.

    Args:
        log_file_name -- path to scanned file,
        string -- sting to be found,
        timout -- block timeout in seconds, after this time exception will be raised.
    """
    logger.info('Waiting for string "{}" in file {}'.format(string, log_file_name))
    step = 1
    to_timeout = 0.0
    from time import sleep
    from os.path import exists

    while True:
        sleep(step)
        to_timeout = to_timeout + step
        if timeout is not None and to_timeout >= timeout:
            msg = "Timeout during wait for string {0}".format(string)
            logger.error(msg)
            raise TimeoutError(msg)
        if exists(log_file_name):
            with open(log_file_name, "r") as log_file:
                leave = False
                for line in log_file.readlines():
                    if string in line:
                        leave = True
                        break
                if leave:
                    break


def get_last_line_of_file(file_name):
    """Reads and returns last line of given file.

    Args:
        file_name -- path to a file.
    """
    last_line = ""
    from os import SEEK_CUR, SEEK_END

    with open(file_name, "r") as f:
        f.seek(-2, SEEK_END)
        while f.read(1) != b"\n":
            f.seek(-2, SEEK_CUR)
        last_line = f.readline().decode()
    return last_line


def kill_process(pid_file_name, proc_name, ip_address, port):
    """Attempts to stop a process with given PID listening on port at ip_address. Process data is read from pid_file_name.

    Args:
        pid_file_name -- path to pid file,
        proc_name -- executable name,
        ip_address -- executable ip address,
        port -- executable port number.
    """
    logger.info("Terminating {0} process running on port {1}".format(proc_name, port))
    pids = []
    pid_name = None
    try:
        from os import popen, kill, remove
        from os.path import exists

        with open(pid_file_name, "r") as pid_file:
            pid_name = pid_file.readline()
            pid_name = pid_name.strip()
        if pid_name is not None:
            for line in popen("ps ax | grep " + proc_name + " | grep -v grep"):
                if pid_name in line:
                    line = line.strip().split()
                    pids.append(line[0])
            for pid in pids:
                for line in popen("ps --no-header --ppid {0}".format(pid)):
                    line = line.strip().split()
                    kill(int(line[0]), 2)
                kill(int(pid), 2)
            if exists(pid_file_name):
                remove(pid_file_name)
            logger.info("Done...")
        else:
            logger.warning("No such process: {0}".format(pid_name))
    except Exception as ex:
        logger.error("Process {0} cannot be killed. Reason: {1}".format(proc_name, ex))


def detect_process_by_name(proc_name, exec_path, port):
    """Checks if  process of given name runs on given ip_address and port.

    Args:
        proc_name -- process name,
        exec_path -- path to executable,
        port -- process port.
    """
    pids = []
    from os import popen

    for line in popen("ps ax | grep " + proc_name + " | grep -v grep"):
        if exec_path in line and str(port) in line:
            line = line.strip().split()
            pids.append(line[0])
    if pids:
        return True
    return False


BLOCK_TYPE_HEADBLOCK = "within_reversible_block"
BLOCK_TYPE_IRREVERSIBLE = "within_irreversible_block"


def block_until_transaction_in_block(node_url, transaction_id, block_type=BLOCK_TYPE_HEADBLOCK, timeout=60.0):
    logger.info("Block until transaction_id: {0} is {1}".format(transaction_id, block_type))
    from time import sleep
    from requests import post

    step = 1.0
    timeout_cnt = 0.0
    while True:
        query = {
            "id": "{}".format(get_random_id()),
            "jsonrpc": "2.0",
            "method": "transaction_status_api.find_transaction",
            "params": {"transaction_id": transaction_id},
        }

        response = post(node_url, json=query)
        transaction_status = response.get("status", None)
        if transaction_status is not None:
            if transaction_status == block_type:
                logger.info("Transaction id: {0} is {1}".format(transaction_id, block_type))
                return
            logger.info("Transaction id: {0} not {1}".format(transaction_id, block_type))
        sleep(step)
        timeout_cnt = timeout_cnt + step
        if timeout_cnt > timeout:
            msg = "Timeout reached during block_until_transaction_in_block"
            logger.error(msg)
            raise TimeoutError(msg)


junit_test_cases = []


def junit_test_case(method):
    def log_test_case(*args, **kw):
        start_time = time.time()
        error = None
        try:
            result = method(*args, **kw)
        except:
            e = sys.exc_info()
            error = traceback.format_exception(e[0], e[1], e[2])
            raise
        finally:
            end_time = time.time()
            test_case = TestCase(method.__name__, method.__name__, end_time - start_time, "", "")
            if error is not None:
                test_case.add_failure_info(output=error)
            junit_test_cases.append(test_case)

    return log_test_case
