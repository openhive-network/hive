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
  parser.add_argument("account", type = str, help = "Name of account")
  parser.add_argument("start", type = int, help = "Start block")
  parser.add_argument("limit", type = int, help = "How many records")

  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("Account: {}".format(args.account))
  print("Start: {}".format(args.start))
  print("Limit: {}".format(args.limit))

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "account_history_api.get_account_history",
    "params": { 
      "account": "{}".format(args.account), 
      "start": "{}".format(args.start), 
      "limit": "{}".format(args.limit)
    }
  }

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)

