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
  parser.add_argument("block_range_begin", type = int, help = "Start")
  parser.add_argument("block_range_end", type = int, help = "End")


  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("Block Range Begin: {}".format(args.block_range_begin))
  print("Block Range End: {}".format(args.block_range_end))

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "account_history_api.enum_virtual_ops",
    "params": { 
      "block_range_begin": "{}".format(args.block_range_begin), 
      "block_range_end": "{}".format(args.block_range_end)
    }
  }

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)

