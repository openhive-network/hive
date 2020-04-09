#!/usr/bin/python3

# Currently unworking argument are commented.

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
  parser.add_argument("start_author", type = str, help = "Author name")
  parser.add_argument("start_permlink", type = str, help = "Permlink to post")
  parser.add_argument("limit", type = int, help = "How many records we want")
  #parser.add_argument("truncate_body", type = int, help = "...")
  #parser.add_argument("filter_tags", type = str, help = "Tags to filter")
  #parser.add_argument("select_authors", type = str, help = "Selected authors")
  #parser.add_argument("select_tags", type = str, help = "Selected tags")

  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  print("Test node: {}".format(args.test_node))
  print("Ref node: {}".format(args.ref_node))
  print("Work dir: {}".format(args.work_dir))
  print("start_author: {}".format(args.start_author))
  print("start_permlink: {}".format(args.start_author))
  print("limit: {}".format(args.limit))
  #print("truncate_body: {}".format(args.truncate_body))
  #print("filter_tags: {}".format(args.filter_tags))
  #print("select_authors: {}".format(args.select_authors))
  #print("select_tags: {}".format(args.select_tags))

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "tags_api.get_discussions_by_comments",
    "params": { 
      "start_author": "{}".format(args.start_author),
      "start_permlink": "{}".format(args.start_permlink),
      "limit": "{}".format(args.limit)
    }
  }
  #"truncate_body": "{}".format(args.truncate_body),
  #"filter_tags": "{}".format(args.filter_tags),
  #"select_authors": "{}".format(args.select_authors),
  #"select_tags": "{}".format(args.select_tags)

  if tester.compare_results(test_args, True):
    exit(0)
  exit(1)

