from __future__ import annotations

import json
import os
import subprocess

import pytest

import test_tools as tt


def sign_transaction(transaction: dict, private_key: str, serialization_type: str = ""):
    # run sign_transaction executable file
    input = f'{{ "tx": {json.dumps(transaction)}, "wif": "{private_key}" '
    if serialization_type:
        input += f', "serialization_type": "{serialization_type}" '
    input += "}"

    output = subprocess.check_output(
        [os.environ["SIGN_TRANSACTION_PATH"]], input=input.encode(encoding="utf-8")
    ).decode("utf-8")
    return json.loads(output)["result"]


@pytest.mark.parametrize(
    ("serialization_type", "expected_signature"),
    [
        (
            None,
            "20cc21e806aac0ca3eca78d2dbbf1768fc3a3ae593c1808193a358c1513dfbcb4b0a8fdff87521d6d3985ad46130bc443fa7d8cc84a166ca4ac0fb346c2e59d431",
        ),
        (
            "legacy",
            "20cc21e806aac0ca3eca78d2dbbf1768fc3a3ae593c1808193a358c1513dfbcb4b0a8fdff87521d6d3985ad46130bc443fa7d8cc84a166ca4ac0fb346c2e59d431",
        ),
        (
            "hf26",
            "1f33646fe77e92ead2a1738493a14070603cb363520d877e0021edfda3513b04c710f6e8817017e723473f3dff8c7f36ea48e2fdf6aefe306c1e1422c98d09d842",
        ),
    ],
)
def test_verify_prepared_signature(serialization_type, expected_signature):
    prepared_transaction = """
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
              "STM6bNK2f2kwNsNFPY1brmBWFUCnesQfrtHzXAR4h6urZB2WvG8AT",
              1
            ]
          ]
        },
        "active": {
          "weight_threshold": 1,
          "account_auths": [],
          "key_auths": [
            [
              "STM6rnWpbK62dMtgYNm834pJPT5cNMPYsY16BLaa91YMtzdBeAAtc",
              1
            ]
          ]
        },
        "posting": {
          "weight_threshold": 1,
          "account_auths": [],
          "key_auths": [
            [
              "STM7uWKRzb5UnEao9o1cPvviZ4mJwBw9frP5pJcK6DpNhn1BKNYBm",
              1
            ]
          ]
        },
        "memo_key": "STM5uwpctKP6bcxhk7poWFqmKSwtwkFrQxFWD8ix7PNr6ekfAd8Wd",
        "json_metadata": "{}"
      }
    }
  ],
  "extensions": [],
  "block_num": 3,
  "transaction_num": 0
}
"""
    sign_transaction_result = sign_transaction(
        json.loads(prepared_transaction), tt.Account("initminer").private_key, serialization_type
    )
    tt.logger.info(f"sign_transaction_result: {json.dumps(sign_transaction_result)}")

    assert sign_transaction_result["sig"] == expected_signature
