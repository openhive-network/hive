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
  parser.add_argument("sort", type = str, help = "Supported sort values: (trending, hot, created, promoted, payout, payout_comments, muted)")
  parser.add_argument("tag", type = str, help = "Any valid tag")
  parser.add_argument("observer", type = str, help = "Any valid account, can be empty string")                                                

  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("Sort by: {}".format(args.sort))
  print("Tag: {}".format(args.tag))
  print("Observer: {}".format(args.observer))


  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "bridge.get_ranked_posts",
    "params": { 
      "sort": "{}".format(args.sort),
      "tag": "{}".format(args.tag),
      "observer": "{}".format(args.observer)
    }
  }

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)

