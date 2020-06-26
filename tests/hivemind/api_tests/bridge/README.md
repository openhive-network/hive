# Bridge methods
- account_notifications
- get_community
- get_ranked_posts
- list_all_subscriptions
- list_community_roles 

#### account_notifications
`python3 ./account_notifications.py https://api.steemit.com https://api.steem.house ./ steemmeupscotty 1`

At any higher value of limit parameter:
````
Traceback (most recent call last):
  File "account_notifications.py", line 40, in <module>
    if tester.compare_results(test_args, True):
  File "..\testbase.py", line 80, in compare_results
    print("Differences detected in jsons: {}".format(self.json_pretty_string(json_diff)))
  File "..\testbase.py", line 39, in json_pretty_string
    return json.dumps(json_obj, sort_keys=True, indent=4)
  File "C:\Python38\lib\json\__init__.py", line 234, in dumps
    return cls(
  File "C:\Python38\lib\json\encoder.py", line 201, in encode
    chunks = list(chunks)
  File "C:\Python38\lib\json\encoder.py", line 431, in _iterencode
    yield from _iterencode_dict(o, _current_indent_level)
  File "C:\Python38\lib\json\encoder.py", line 405, in _iterencode_dict
    yield from chunks
  File "C:\Python38\lib\json\encoder.py", line 353, in _iterencode_dict
    items = sorted(dct.items())
TypeError: '<' not supported between instances of 'Symbol' and 'int'
````

With arguments(tarazkp 22)
Detected id differences and sometimes there are differences in whole posts
````
(other id differences)
        "18": {
            "id": 72563412
        },
        "19": {
            "id": 72562841
        },
        "20": {
            "date": "2020-03-11T02:06:30",
            "id": 72562118,
            "msg": "@santosneto resteemed your post",
            "score": 30,
            "type": "reblog",
            "url": "@tarazkp/a-year-dangerous-code-and-an-orca-between"
        },
        "21": {
            "date": "2020-03-11T01:58:18",
            "id": 72561713,
            "msg": "@edicted voted on your post ($0.32)",
            "score": 50,
            "url": "@tarazkp/as-they-say-location-location-location"
        }
    }
````
With arguments (harkar 40)
````
		"27": {
            "id": 72555897
        },
        "28": {
            "id": 72552007
        },
        "29": {
            "date": "2020-03-10T20:48:30",
            "id": 72548815,
            "msg": "@dalz voted on your post ($0.03)",
            "score": 25
        },
        "30": {
            "date": "2020-03-10T20:46:42",
            "id": 72548814,
            "msg": "@faltermann voted on your post ($0.03)"
        },
        "31": {
            "date": "2020-03-10T21:32:21",
            "id": 72548813,
            "msg": "@czechglobalhosts voted on your post ($0.67)",
            "score": 50
        },
        "32": {
            "id": 72547716
        },
        "33": {
            "date": "2020-03-10T20:02:21",
            "id": 72547072,
            "msg": "@shaikmashud replied to your post",
            "score": 40,
            "type": "reply",
            "url": "@shaikmashud/q6zubx"
        },
        "34": {
            "date": "2020-03-10T19:58:15",
            "id": 72546999,
            "msg": "@karja replied to your post",
            "score": 60,
            "type": "reply",
            "url": "@karja/q6zu53"
        },
        "35": {
            "date": "2020-03-10T19:50:48",
            "id": 72546848,
            "msg": "@uadigger replied to your post",
            "score": 50,
            "type": "reply",
            "url": "@uadigger/q6ztsn"
        },
        "36": {
            "date": "2020-03-10T18:40:27",
            "id": 72543466,
            "msg": "@investinthefutur replied to your post",
            "type": "reply",
            "url": "@investinthefutur/re-skiing-in-the-black-valley-20200310t184024z"
        },
        "37": {
            "date": "2020-03-10T18:40:21",
            "id": 72543455,
            "msg": "@voinvote3 voted on your post ($0.06)",
            "url": "@harkar/skiing-in-the-black-valley"
        },
        "38": {
            "date": "2020-03-10T18:36:39",
            "id": 72543326,
            "msg": "@coffeea.token replied to your post",
            "score": 50,
            "type": "reply",
            "url": "@coffeea.token/re-skiing-in-the-black-valley-20200310t183637z"
        },
        "39": {
            "date": "2020-03-10T18:36:33",
            "id": 72543321,
            "msg": "@beerlover replied to your post",
            "score": 50,
            "type": "reply",
            "url": "@beerlover/re-skiing-in-the-black-valley-20200310t183631z"
        }
````

#### get_community
`python3 ./get_community.py https://api.steemit.com https://api.steem.house ./ hive-123456 alice`

Detected differences 
-> https://api.steemit.com: "id": 1332149
-> https://api.steem.house: "id": 1332614

`"hive-193552" "roknavy"
Detected differences
-> https://api.steemit.com: "id": 1344430
-> https://api.steem.house: "id": 1344895

-> https://api.steemit.com: "sum_pending": 1217
-> https://api.steem.house: "sum_pending": 1213

Except that difference everything is fine.

#### get_ranked_posts
`python3 ./get_ranked_posts.py https://api.steemit.com https://api.steem.house ./ trending steem alice`

With any arguments i tried:

Response from https://api.steemit.com:
is a huge wall of text
````
Response from ref node at https://api.steem.house:
{
    "error": {
        "code": -32602,
        "data": "unsupported operand type(s) for -: 'float' and 'NoneType'",
        "message": "Invalid parameters"
    },
    "id": 1,
    "jsonrpc": "2.0"
}
Traceback (most recent call last):
  File "get_ranked_posts.py", line 44, in <module>
    if tester.compare_results(test_args, True):
  File "..\testbase.py", line 80, in compare_results
    print("Differences detected in jsons: {}".format(self.json_pretty_string(json_diff)))
  File "..\testbase.py", line 39, in json_pretty_string
    return json.dumps(json_obj, sort_keys=True, indent=4)
  File "C:\Python38\lib\json\__init__.py", line 234, in dumps
    return cls(
  File "C:\Python38\lib\json\encoder.py", line 201, in encode
    chunks = list(chunks)
  File "C:\Python38\lib\json\encoder.py", line 431, in _iterencode
    yield from _iterencode_dict(o, _current_indent_level)
  File "C:\Python38\lib\json\encoder.py", line 353, in _iterencode_dict
    items = sorted(dct.items())
TypeError: '<' not supported between instances of 'Symbol' and 'str'
````

#### list_all_subscriptions
`python3 ./list_all_subscriptions.py https://api.steemit.com https://api.steem.house ./ alice`

Works fine.

#### list_community_roles
`python3 ./list_community_roles.py https://api.steemit.com https://api.steem.house ./ blocktrades`

Everything works fine.