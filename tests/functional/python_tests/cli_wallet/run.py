#!/usr/bin/env python3
import os
import sys
import glob
import argparse
import datetime
import subprocess
import time
import sys

from junit_xml import TestCase, TestSuite

import sys


from tests.utils.cmd_args import parser
from tests.utils.logger import log
from tests.utils.node_util import start_node

test_args = []
junit_test_cases=[]
summary_file_name = "summary.txt"

def check_subdirs(_dir):
  error = False
  tests = sorted(glob.glob(_dir+"/*.py"))
  if tests:
    for test in tests:
      root_error = run_script(test)
      if root_error:
        error = root_error
  return error


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
    if os.path.isfile(summary_file_name):
        os.remove(summary_file_name)
    with open(summary_file_name, "a+") as summary:
        summary.writelines("Cli wallet test started at {0}.\n".format(str(datetime.datetime.now())[:-7]))
    args = parser.parse_args()
    for key, val in args.__dict__.items():
        if key in ["hive_path", "hive_working_dir", "hive_config_path"]:
            continue
        if val :
            test_args.append("--"+key.replace("_","-")+ " ")
            test_args.append(val)
    test_args = " ".join(test_args)
    try:
        node=start_node()
        error = True
        if os.path.isdir("./tests"):
            error = check_subdirs("./tests")

    except Exception as _ex:
        log.exception("Exception occured `{0}`.".format(str(_ex)))
        error = True
    finally:
        if node:
            node.stop_hive_node()
        if error:
            log.error("At least one test has faild. Please check summary.txt file.")
            exit(1)
        else:
            log.info("All tests pass.")
            exit(0)
