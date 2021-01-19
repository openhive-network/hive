#!/usr/bin/env python3
import os
import sys
import glob
import argparse
import datetime
import subprocess
import time
import sys
import json
from shutil import rmtree

from junit_xml import TestCase, TestSuite

from tests.utils.cmd_args import parser
from tests.utils.logger import log
from tests.utils.node_util import start_node

sys.path.append("../../")
from hive_utils.resources.configini import config

test_args = []
junit_test_cases=[]
summary_file_name = "summary.txt"
test_filter=None

def check_subdirs(_dir):
  error = False
  tests = sorted(glob.glob(_dir+"/*.py"))
  if tests:
    if test_filter:
      print("Tests filtered with {}".format(test_filter))
    else:
      print("Tests not filtered")
    for test in tests:
      if not test_filter or test_filter in test:
        root_error = run_script(test)
        if root_error:
          error = root_error
  return error

def prepare_config(args):
  global test_filter
  def strip_address( addr : str ):
    return addr.split('/')[2]

  cfg = config()

  # required plugins
  cfg.update_plugins(["account_by_key", "account_by_key_api",
    "account_history", "account_history_api",
    "block_api", "condenser_api", "database_api", 
    "debug_node_api", "json_rpc", 
    "network_broadcast_api", "p2p", 
    "rc", "rc_api", "transaction_status", 
    "transaction_status_api", "webserver", "witness",
    "market_history", "market_history_api",
    "account_history_rocksdb", "account_history_api", "wallet_bridge_api" ])

  # option required by user
  cfg.webserver_http_endpoint = strip_address(args.server_http_endpoint)
  cfg.webserver_ws_endpoint = strip_address(args.server_rpc_endpoint)

  datadir = args.hive_working_dir
  if not os.path.exists(datadir):
    os.mkdir(datadir)
  blockchain_dir = os.path.join(datadir, "blockchain")
  if os.path.exists(blockchain_dir):
    rmtree(blockchain_dir)
  cfg.generate(os.path.join(datadir, "config.ini"))
  if "test_filter" in args:
    test_filter = args.test_filter

def run_script(_test, _multiplier = 1, _interpreter = None ):
  try:
      with open(summary_file_name, "a+") as summary:
        interpreter = _interpreter if _interpreter else "python3"
        start_time=time.time()
        out = subprocess.run(interpreter + " " + _test + " " + test_args, shell=True, stderr=subprocess.PIPE)
        end_time=time.time()
        test_case=TestCase(_test, _test, end_time - start_time, '', '')
        junit_test_cases.append(test_case)
        if out.stderr:
            test_case.add_failure_info(output = out.stderr)
            summary.writelines("Test `{0}` failed.\n".format(_test))
            return True
        else:
            summary.writelines("Test `{0}` passed.\n".format(_test))
            return False
  except Exception as _ex:
    log.exception("Exception occures in run_script `{0}`".format(str(_ex)))
    return True


if __name__ == "__main__":
    node = None
    if os.path.isfile(summary_file_name):
        os.remove(summary_file_name)
    with open(summary_file_name, "a+") as summary:
        summary.writelines("Cli wallet test started at {0}.\n".format(str(datetime.datetime.now())[:-7]))
    args = parser.parse_args()
    for key, val in args.__dict__.items():
        if key in ["hive_path", "hive_working_dir", "hive_config_path", "rpc_allowip", "test_filter"] or isinstance(val, bool):
            continue
        if val:
            test_args.append("--"+key.replace("_","-")+ " ")
            test_args.append(val)
    print(test_args)
    test_args = " ".join(test_args)
    try:
        prepare_config(args)
        node = start_node(run_using_existing_data=True)
        error = False
        if os.path.isdir("./tests"):
            error = check_subdirs("./tests")

    except Exception as _ex:
        log.exception("Exception occured `{0}`.".format(str(_ex)))
        error = True
    finally:
        if node:
            node.stop_hive_node()
        if args.junit_output:
            test_suite = TestSuite('cli_wallet_test', junit_test_cases)
            with open(args.junit_output, "w") as junit_xml:
                TestSuite.to_file(junit_xml, [test_suite], prettyprint=False)
        if error:
            log.error("At least one test has faild. Please check summary.txt file.")
            exit(1)
        else:
            log.info("All tests pass.")
            exit(0)
