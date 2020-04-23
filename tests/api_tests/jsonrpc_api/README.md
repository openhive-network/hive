# Jsonrpc Api 
- get_methods
- get_signature

#### get_methods
###### Returns a list of methods supported by the JSON RPC API.
Example call:

`python get_methods.py https://api.steemit.com https://api.steem.house ./`

Result: Some differences between jsons, python returns errors.
```
Traceback (most recent call last):
  File "get_methods.py", line 33, in <module>
    if tester.compare_results(test_args, True):
  File "..\testbase.py", line 83, in compare_results
    print("Differences detected in jsons: {}".format(self.json_pretty_string(json_diff)))
  File "..\testbase.py", line 39, in json_pretty_string
    return json.dumps(json_obj, sort_keys=True, indent=4)
  File "P:\dev_tools\Python\lib\json\__init__.py", line 234, in dumps
    return cls(
  File "P:\dev_tools\Python\lib\json\encoder.py", line 201, in encode
    chunks = list(chunks)
  File "P:\dev_tools\Python\lib\json\encoder.py", line 431, in _iterencode
    yield from _iterencode_dict(o, _current_indent_level)
  File "P:\dev_tools\Python\lib\json\encoder.py", line 405, in _iterencode_dict
    yield from chunks
  File "P:\dev_tools\Python\lib\json\encoder.py", line 353, in _iterencode_dict
    items = sorted(dct.items())
TypeError: '<' not supported between instances of 'Symbol' and 'Symbol'
``` 

#### get_signature
###### Returns the signature information for a JSON RPC method including the arguments and expected response JSON.
Example call:

`python get_signature.py https://api.steemit.com https://api.steem.house ./ market_history_api.get_market_history`

Result: no differences.