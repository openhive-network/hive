# Tags Api
- get_active_votes
- get_comment_discussions_by_payout
- get_content_replies
- get_discussion
- get_discussions_by_active
- get_discussions_by_author_before_date
- get_discussions_by_blog
- get_discussions_by_cashout
- get_discussions_by_children
- get_discussions_by_comments
- get_discussions_by_created
- get_discussions_by_feed
- get_discussions_by_hot
- get_discussions_by_promoted
- get_discussions_by_trending
- get_discussions_by_votes
- get_post_discussions_by_payout
- get_replies_by_last_update
- get_tags_used_by_author
- get_trending_tags

#### get_active_votes
###### Returns all votes for the given post.
Both nodes return error `Method not found` (no differences).
```
{
    "error": {
        "code": -32601,
        "data": "tags_api.get_active_votes",
        "message": "Method not found"
    },
    "id": 1,
    "jsonrpc": "2.0"
}
```

Example call: 

`python3 get_active_votes.py https://api.steemit.com https://api.steem.house ./ flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3`

#### get_comment_discussions_by_payout 
##### Returns a list of discussions based on payout.
It works if I use only two args, tag and limit, eq:

`python3 get_comment_discussions_by_payout.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

But there are differences in results.

When I added list arguments (even as single str) filter_tags, select_authors select_tags, I got response:

```
{
    "error": {
        "code": -32602,
        "data": "too many keyword arguments {'filter_tags': '[]', 'select_authors': '[]', 'select_tags': '[]'}",
        "message": "Invalid parameters"
    },
    "id": 1,
    "jsonrpc": "2.0"
}
```

Probably that parameters, the same for a lot of methods, aren't handled by that API 

#### get_content_replies
###### Returns a list of replies.
Differences in post_id.
 
Example call: 

`python3 get_content_replies.py https://api.steemit.com  https://api.steem.house ./ flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3`


#### get_discussion
###### Returns the discussion given an author and permlink.
`steemmeupscotty black-dog-on-a-hong-kong-sunrise-animal-landscape-photography`

Difference in results/json_metadata and results/post_id. Other are good. 

#### get_discussions_by_active
##### Returns a list of discussions based on active.
`blocktrades 1`

Method not found in API.

#### get_discussions_by_author_before_date
###### Returns a list of discussions based on author before date.
Differences in post_id.

Example call:

`python3 get_discussions_by_author_before_date.py https://api.steemit.com https://api.steem.house ./ 
flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3 2020-03-01T00:00:00 1`

#### get_discussions_by_blog
##### Returns a list of discussions by blog.
`python3 get_discussions_by_blog.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

Works fine, excepts error with arguments:

```
    "error": {
        "code": -32602,
        "data": "too many keyword arguments {'filter_tags': '[]', 'select_authors': '[]', 'select_tags': '[]'}",
        "message": "Invalid parameters"
    },
```

#### get_discussions_by_cashout
##### Returns a list of discussions by cashout.
`python3 get_discussions_by_cashout.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

Method not found.

#### get_discussions_by_children
##### Returns a list of discussions by children.
`blocktrades` `1`

Method not found.

#### get_discussions_by_comments
##### Returns a list of discussions by comments.
`python3 get_discussions_by_comments.py https://api.steemit.com https://api.steem.house ./ steemmeupscotty black-dog-on-a-hong-kong-sunrise-animal-landscape-photography 1`

Method works fine, we have comparision error in `active_votes` `json_metadata` and `post_id`.

Documentation isn't correct, instead `tag` argument there are `start_author` and `start_permlink`.

#### get_discussions_by_created
##### Returns a list of discussions by created.
`python3 get_discussions_by_created.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

Method works fine, comparision error in `post_id` only.

####get_discussions_by_feed
#####Returns a list of discussions by feed.
`python3 get_discussions_by_feed.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

Method not found.

####get_discussions_by_hot. 
##### Returns a list of discussions by hot.
`python3 get_discussions_by_hot.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

Method works fine, comparision error in `post_id` only.

#### get_discussions_by_promoted
##### Returns a list of discussions by promoted.
`python3 get_discussions_by_promoted.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

Method works fine, but results are empty.

#### get_discussions_by_trending
##### Returns a list of discussions by trending.
`python3 get_discussions_by_trending.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

Method works fine for params with empty results as return. Otherwise python3 returns error:

```
Traceback (most recent call last):
  File "./get_discussions_by_trending.py", line 54, in <module>
    if tester.compare_results(test_args, True):
  File "../testbase.py", line 83, in compare_results
    print("Differences detected in jsons: {}".format(self.json_pretty_string(json_diff)))
  File "../testbase.py", line 39, in json_pretty_string
    return json.dumps(json_obj, sort_keys=True, indent=4)
  File "/usr/lib/python33.6/json/__init__.py", line 238, in dumps
    **kw).encode(obj)
  File "/usr/lib/python33.6/json/encoder.py", line 201, in encode
    chunks = list(chunks)
  File "/usr/lib/python33.6/json/encoder.py", line 430, in _iterencode
    yield from _iterencode_dict(o, _current_indent_level)
  File "/usr/lib/python33.6/json/encoder.py", line 404, in _iterencode_dict
    yield from chunks
  File "/usr/lib/python33.6/json/encoder.py", line 404, in _iterencode_dict
    yield from chunks
  File "/usr/lib/python33.6/json/encoder.py", line 404, in _iterencode_dict
    yield from chunks
  File "/usr/lib/python33.6/json/encoder.py", line 376, in _iterencode_dict
    raise TypeError("key " + repr(key) + " is not a string")
TypeError: key insert is not a string
```

#### get_discussions_by_votes
##### Returns a list of discussions by votes.
`python3 get_discussions_by_votes.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

Method not found.

#### get_post_discussions_by_payout
##### Returns a list of post discussions by payout.
`python3 get_post_discussions_by_payout.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`

Method works fine, but there is difference in comparition:

```
Differences detected in jsons: {
    "result": {
        "0": {
            "body": "",
            "body_length": 0,
            "json_metadata": "{}",
            "post_id": 84964301,
            "root_title": "",
            "title": ""
        }
    }
}
```


#### get_replies_by_last_update
###### Returns a list of replies by last update.
Both nodes return error `Method not found` (no differences).
```
{
    "error": {
        "code": -32601,
        "data": "tags_api.get_replies_by_last_update",
        "message": "Method not found"
    },
    "id": 1,
    "jsonrpc": "2.0"
}
```
Example call:

`python3 get_replies_by_last_update.py https://api.steemit.com https://api.steem.house ./ flaws my-first-experience-integrating-steem-into-chess-in-my-state-or-a-lot-of-photos-3 1`
 
#### get_tags_used_by_author
###### Returns a list of tags used by an author.
Both nodes return error `Method not found` (no differences).
```
{
    "error": {
        "code": -32601,
        "data": "tags_api.get_tags_used_by_author",
        "message": "Method not found"
    },
    "id": 1,
    "jsonrpc": "2.0"
}
```

Example call:

`python3 get_tags_used_by_author.py https://api.steemit.com https://api.steem.house ./ flaws`

#### get_trending_tags
###### Returns the list of trending tags.
Both nodes return error `Method not found` (no differences).
```
{
    "error": {
        "code": -32601,
        "data": "tags_api.get_trending_tags",
        "message": "Method not found"
    },
    "id": 1,
    "jsonrpc": "2.0"
}
```

Example call:

`python3 get_trending_tags.py https://api.steemit.com https://api.steem.house ./ blocktrades 1`



