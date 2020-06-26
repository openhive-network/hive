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
  parser.add_argument("account", type = str, help = "Account name")
  parser.add_argument("start", type = str, help = "Account to start from (default is null)")
  parser.add_argument("type", type = str, help = "Type")
  parser.add_argument("limit", type = str, help = "Limit up to 1000")

  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("Account name: {}".format(args.account))
  print("Account to start from: {}".format(args.start))
  print("Type: {}".format(args.type))
  print("Limit: {}".format(args.limit))

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "condenser_api.get_following",
    "params": [
      "{}".format(args.account),
      "{}".format(args.start),
      "{}".format(args.type),
      args.limit
    ]
  }

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)

