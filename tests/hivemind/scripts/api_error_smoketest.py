#!/usr/bin/env python3

import sys
import requests
import json

tests = [
   {
      "method": "condenser_api.get_state",
      "params": ["trending"]
   },
   {
      "method": "condenser_api.get_state",
      "params": ["hot"]
   },
   {
      "method": "condenser_api.get_state",
      "params": ["/@temp"]
   },
   {
      "method": "condenser_api.get_trending_tags",
      "params": ["", 20]
   },
   {
      "method": "condenser_api.get_accounts",
      "params": [["initminer","temp"]]
   },
   {
      "method": "condenser_api.get_active_votes",
      "params": ["temp", "test1"]
   },
   {
      "method": "condenser_api.get_account_votes",
      "params": ["temp"]
   },
   {
      "method": "condenser_api.get_content",
      "params": ["temp", "test1"]
   },
   {
      "method": "condenser_api.get_content_replies",
      "params": ["temp", "foobar"]
   },
   {
      "method": "condenser_api.get_tags_used_by_author",
      "params": ["temp"]
   },
   {
      "method": "condenser_api.get_comment_discussions_by_payout",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_trending",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_created",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_active",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_cashout",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_votes",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_children",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_hot",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_feed",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_blog",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_discussions_by_comments",
      "params": [{"tag":"test", "start_author":"temp"}]
   },
   {
      "method": "condenser_api.get_discussions_by_promoted",
      "params": [{"tag":"test"}]
   },
   {
      "method": "condenser_api.get_replies_by_last_update",
      "params": ["temp", "test1", 10]
   },
   {
      "method": "condenser_api.get_followers",
      "params": ["temp", "", "blog", 10]
   },
   {
      "method": "condenser_api.get_following",
      "params": ["temp", "", "blog", 10]
   },
   {
      "method": "condenser_api.get_follow_count",
      "params": ["temp"]
   },
   {
      "method": "condenser_api.get_feed_entries",
      "params": ["temp", -1, 10]
   },
   {
      "method": "condenser_api.get_feed",
      "params": ["temp", -1, 10]
   },
   {
      "method": "condenser_api.get_blog_entries",
      "params": ["temp", -1, 10]
   },
   {
      "method": "condenser_api.get_blog",
      "params": ["temp", -1, 10]
   },
   {
      "method": "condenser_api.get_account_reputations",
      "params": ["", 10]
   },
# Might be broken
#   {
#      "method": "condenser_api.get_reblogged_by",
#      "params": ["temp", "test1"]
#   },
   {
      "method": "condenser_api.get_blog_authors",
      "params": ["temp"]
   },
   {
      "method": "account_history_api.get_ops_in_block",
      "params": {"block_num":10000}
   },
   {
      "method": "account_history_api.get_ops_in_block",
      "params": {"block_num":10000, "only_virtual":True}
   },
   {
      "method": "follow_api.get_followers",
      "params": {"account":"temp", "start":"", "type":"blog", "limit":10}
   },
   {
      "method": "follow_api.get_following",
      "params": {"account":"temp", "start":"", "type":"blog", "limit":10}
   },
   {
      "method": "follow_api.get_follow_count",
      "params": {"account":"temp"}
   },
   {
      "method": "follow_api.get_feed_entries",
      "params": {"account":"temp", "start":-1, "limit":10}
   },
   {
      "method": "follow_api.get_feed",
      "params": {"account":"temp", "start":-1, "limit":10}
   },
   {
      "method": "follow_api.get_blog_entries",
      "params": {"account":"temp", "start":-1, "limit":10}
   },
   {
      "method": "follow_api.get_blog",
      "params": {"account":"temp", "start":-1, "limit":10}
   },
   {
      "method": "follow_api.get_blog_authors",
      "params": {"blog_account":"temp"}
   },
   {
      "method": "tags_api.get_trending_tags",
      "params": {"start_tag":"", "limit":20}
   },
   {
      "method": "tags_api.get_tags_used_by_author",
      "params": {"author":"temp"}
   },
   {
      "method": "tags_api.get_discussion",
      "params": {"author":"temp", "permlink":"test1"}
   },
   {
      "method": "tags_api.get_content_replies",
      "params": {"author":"temp", "permlink":"foobar"}
   },
   {
      "method": "tags_api.get_post_discussions_by_payout",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_comment_discussions_by_payout",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_trending",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_created",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_active",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_cashout",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_votes",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_children",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_hot",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_feed",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_blog",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_discussions_by_comments",
      "params": {"tag":"test", "start_author":"temp"}
   },
   {
      "method": "tags_api.get_discussions_by_promoted",
      "params": {"tag":"test"}
   },
   {
      "method": "tags_api.get_replies_by_last_update",
      "params": {"start_parent_author":"temp", "start_permlink":"test1", "limit":10}
   },
# Might be broken
#   {
#      "method": "tags_api.get_discussions_by_author_before_date",
#      "params": {"author":"temp", "start_permlink":"test1", "before_date":"1969-12-31T23:59:59", "limit":10}
#   },
   {
      "method": "tags_api.get_active_votes",
      "params": {"author":"temp", "permlink":"test1"}
   },
   {
      "method": "database_api.list_comments",
      "params": {"start":("1970-01-01T00:00:00","",""), "limit":10, "order":"by_cashout_time"}
   },
   {
      "method": "database_api.find_comments",
      "params": {"start":[["temp","test1"],["temp","foobar"]], "limit":10, "order":"by_account"}
   },
   {
      "method": "database_api.list_votes",
      "params": {"start":["","",""], "limit":10, "order":"by_comment_voter"}
   },
   {
      "method": "database_api.find_votes",
      "params": {"author":"temp", "permlink":"test1"}
   }
]

def test_api( url, headers, payload ):
   response = requests.post( url, data=json.dumps(payload), headers=headers).json()

   try:
      if( response["id"] != payload["id"]
         or response["jsonrpc"] != "2.0" ):
         return False
      response["result"]
   except:
      return False

   try:
      response["error"]
      return False
   except KeyError:
      return True
   except:
      return False

   return True

def main():
   if len( sys.argv ) == 1:
      url = "https://api.steemit.com/"
   elif len( sys.argv ) == 2:
      url = sys.argv[1]
   else:
      exit( "Usage: api_error_smoketest.py <hive_api_endpoint>" )

   print( "Testing against endpoint: " + url )

   headers = {'content-type': 'application/json'}

   payload = {
      "jsonrpc": "2.0"
   }

   id = 0
   errors = 0

   for testcase in tests:
      payload["method"] = testcase["method"]
      payload["params"] = testcase["params"]
      payload["id"] = id

      if( not test_api( url, headers, payload ) ):
         errors += 1
         print( "Error in testcase: " + json.dumps( payload ) )

      id += 1

   print( str( errors ) + " error(s) found." )

   if( errors > 0 ):
      return 1

   return 0

if __name__ == "__main__":
   main()
