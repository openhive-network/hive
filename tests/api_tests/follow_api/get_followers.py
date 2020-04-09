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
  parser.add_argument("block_number", type = int, help = "Block to compare")
  parser.add_argument("account_name", type = str, help = "Account name")
  parser.add_argument("start", type = str, help = "Start")
  parser.add_argument("type", type = str, help = "Type")
  parser.add_argument("limit", type = str, help = "Limit")

  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("Block number: {}".format(args.block_number))

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "follow_api.get_followers",
    "params": {
      "account": args.account_name,
      "start": args.start,
      "type": args.type,
      "limit": args.limit
    }
  }

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)