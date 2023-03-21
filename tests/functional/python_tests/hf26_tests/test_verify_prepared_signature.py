import json
import pytest
import os
import subprocess

import test_tools as tt

def sign_transaction(transaction: dict, private_key: str, serialization_type: str = ''):
    # run sign_transaction executable file
    input = f'{{ "tx": {json.dumps(transaction)}, "wif": "{private_key}" '
    if serialization_type:
        input += f', "serialization_type": "{serialization_type}" '
    input += f'}}'

    output = subprocess.check_output([os.environ['SIGN_TRANSACTION_PATH']],
                                     input=input.encode(encoding='utf-8')).decode('utf-8')
    return json.loads(output)['result']

@pytest.mark.parametrize(
    'serialization_type,expected_signature', [
        (None,'1f6392e3c5fd8a13d0979a37deb68e8141f54365ffc2941dea4dfe95d848fb79a43644bf471f40891c487de3bdf24c772e3cc8ffc852e9b494fbd40c8e34ea7411'),
        ('legacy','1f6392e3c5fd8a13d0979a37deb68e8141f54365ffc2941dea4dfe95d848fb79a43644bf471f40891c487de3bdf24c772e3cc8ffc852e9b494fbd40c8e34ea7411'),
        ('hf26','1f33646fe77e92ead2a1738493a14070603cb363520d877e0021edfda3513b04c710f6e8817017e723473f3dff8c7f36ea48e2fdf6aefe306c1e1422c98d09d842'),
    ]
)
def test_verify_prepared_signature(serialization_type, expected_signature):
    prepared_transaction = '''
{
  "ref_block_num": 2,
  "ref_block_prefix": 1106878340,
  "expiration": "2022-07-26T09:33:33",
  "operations": [
    {
      "type": "account_create_operation",
      "value": {
        "fee": {
          "amount": "0",
          "precision": 3,
          "nai": "@@000000021"
        },
        "creator": "initminer",
        "new_account_name": "alice",
        "owner": {
          "weight_threshold": 1,
          "account_auths": [],
          "key_auths": [
            [
              "TST6bNK2f2kwNsNFPY1brmBWFUCnesQfrtHzXAR4h6urZB2WvG8AT",
              1
            ]
          ]
        },
        "active": {
          "weight_threshold": 1,
          "account_auths": [],
          "key_auths": [
            [
              "TST6rnWpbK62dMtgYNm834pJPT5cNMPYsY16BLaa91YMtzdBeAAtc",
              1
            ]
          ]
        },
        "posting": {
          "weight_threshold": 1,
          "account_auths": [],
          "key_auths": [
            [
              "TST7uWKRzb5UnEao9o1cPvviZ4mJwBw9frP5pJcK6DpNhn1BKNYBm",
              1
            ]
          ]
        },
        "memo_key": "TST5uwpctKP6bcxhk7poWFqmKSwtwkFrQxFWD8ix7PNr6ekfAd8Wd",
        "json_metadata": "{}"
      }
    }
  ],
  "extensions": [],
  "block_num": 3,
  "transaction_num": 0
}
'''
    sign_transaction_result = sign_transaction(
        json.loads(prepared_transaction), tt.Account('initminer').keys.private, serialization_type
    )
    tt.logger.info(f'sign_transaction_result: {json.dumps(sign_transaction_result)}')

    assert sign_transaction_result['sig'] == expected_signature
