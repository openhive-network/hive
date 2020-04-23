#!/usr/bin/python3

import os
import sys

sys.path.append("../")
import json
from testbase import SimpleJsonTest

if __name__ == "__main__":
  import argparse
  parser = argparse.ArgumentParser()

  parser.add_argument("test_node", type = str, help = "IP address of test node")
  parser.add_argument("ref_node", type = str, help = "IP address of reference node")
  parser.add_argument("work_dir", type = str, help = "Work dir")
  parser.add_argument("acc_low_bound", type = str, help = "Lower bound to search accounts")
  parser.add_argument("limit", type = int, help = "Result limits up to 1000")

  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("Account lower bound: {}".format(args.acc_low_bound))
  print("Results limit: {}".format(args.limit))

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "condenser_api.get_account_reputations",
    "params": [
      "{}".format(args.acc_low_bound),
      args.limit
    ]
  }

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)

