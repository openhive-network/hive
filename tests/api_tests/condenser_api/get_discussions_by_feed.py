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
  parser.add_argument("tag", type = str, help = "Tag to organise posts")
  parser.add_argument("start_author", type = str, help = "Start author")
  parser.add_argument("start_permlink", type = str, help = "Start permlink")
  parser.add_argument("limit", type = int, help = "Limit")


  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("Tag: {}".format(args.tag))
  print("Start author: {}".format(args.start_author))
  print("Start permlink: {}".format(args.start_permlink))
  print("Limit: {}".format(args.limit))

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "condenser_api.get_discussions_by_feed",
    "params": [{
      "tag": "{}".format(args.tag),
      "start_author": "{}".format(args.start_author),
      "start_permlink": "{}".format(args.start_permlink),
      "limit": args.limit
    }]
  }

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)

