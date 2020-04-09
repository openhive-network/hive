# methods

## get_account_history
`python3 ./get_account_history.py https://api.steemit.com https://api.steem.house ./ steemmeupscotty 1000 10`

For argument above eveyrthing is alright.

But if limit is too big:
``steemmeupscotty` `10000` `5000``
We have diff with timestamp sometimes.

## get_ops_in_block
`python3 ./get_ops_in_block.py https://api.steemit.com https://api.steem.house ./ 100004`
Worked fine, no problems in compare.
`only_virtual` is set to False by default

## get_transaction
`python3 ./get_transaction.py https://api.steemit.com https://api.steem.house ./ 0000000000000000000000000000000000000000`

DEPRECATED

```
{
    "error": {
        "code": -32003,
        "data": {
            "code": 10,
            "message": "Assert Exception",
            "name": "assert_exception",
            "stack": [
                {
                    "context": {
                        "file": "account_history_api.cpp",
                        "hostname": "",
                        "level": "error",
                        "line": 169,
                        "method": "get_transaction",
                        "timestamp": "2020-03-12T10:09:09"
                    },
                    "data": {},
                    "format": "false: This API is not supported for account history backed by RocksDB"
                }
            ]
        },
        "message": "Assert Exception:false: This API is not supported for account history backed by RocksDB"
    },
    "id": 1,
    "jsonrpc": "2.0"
}

```

## enum_virtual_ops
`python3 ./enum_virtual_ops.py https://api.steemit.com https://api.steem.house ./ 9000 9100`

Worked fine, compare passed.