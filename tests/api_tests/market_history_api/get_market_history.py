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
  parser.add_argument("start", type = str, help = "Start date")
  parser.add_argument("end", type = str, help = "End date")

  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("Bucket seconds: {}".format(args.bucket_seconds))
  print("Start: {}".format(args.start))
  print("End: {}.".format(args.end))

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "market_history_api.get_market_history",
    "params": {
      "bucket_seconds": "{}".format(args.bucket_seconds),
      "start": "{}".format(args.start),
      "end": "{}".format(args.end)
    }
  }

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)

