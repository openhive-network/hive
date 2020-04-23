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
  parser.add_argument("bucket_seconds", type = int, help = "Bucket seconds")
  parser.add_argument("start_timestmp", type = str, help = "Timestamp to start from")
  parser.add_argument("end_timestmp", type = str, help = "Timestamp to end")

  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("Bucket seconds: {}".format(args.bucket_seconds))
  print("Start time: {}".format(args.start_timestmp))
  print("End time: {}".format(args.end_timestmp))

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "condenser_api.get_market_history",
    "params": [
      args.bucket_seconds,
      args.start_timestmp,
      args.end_timestmp
    ]
  }

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)

