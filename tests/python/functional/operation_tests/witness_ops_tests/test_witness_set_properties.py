from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import WitnessAccount


@pytest.mark.parametrize(
    "props_to_serialize",
    [
        # test case 1.1 from https://gitlab.syncad.com/hive/hive/-/issues/635
        {
            "account_creation_fee": {"amount": "28000", "precision": 3, "nai": "@@000000021"},
            "key": tt.Account("alice").public_key,
        },
        # test case 1.2 from https://gitlab.syncad.com/hive/hive/-/issues/635
        {"account_subsidy_budget": 797, "key": tt.Account("alice").public_key},
        # test case 1.3 from https://gitlab.syncad.com/hive/hive/-/issues/635
        {"account_subsidy_decay": 347321, "key": tt.Account("alice").public_key},
        # test case 1.4 from https://gitlab.syncad.com/hive/hive/-/issues/635
        {"maximum_block_size": 131072, "key": tt.Account("alice").public_key},
        # test case 1.5 from https://gitlab.syncad.com/hive/hive/-/issues/635
        {"hbd_interest_rate": 1000, "key": tt.Account("alice").public_key},
        # test case 1.6 from https://gitlab.syncad.com/hive/hive/-/issues/635
        {
            "hbd_exchange_rate": {
                "base": {"amount": "100000", "precision": 3, "nai": "@@000000013"},
                "quote": {"amount": "100000", "precision": 3, "nai": "@@000000021"},
            },
            "key": tt.Account("alice").public_key,
        },
        # test case 1.7 from https://gitlab.syncad.com/hive/hive/-/issues/635
        {"url": "http://new-url.html", "key": tt.Account("alice").public_key},
        # test case 1.8 from https://gitlab.syncad.com/hive/hive/-/issues/635
        {"new_signing_key": tt.Account("alice-new").public_key, "key": tt.Account("alice").public_key},
        # test case 1.9 from https://gitlab.syncad.com/hive/hive/-/issues/635
        {
            "account_creation_fee": {"amount": "28000", "precision": 3, "nai": "@@000000021"},
            "maximum_block_size": 131072,
            "hbd_interest_rate": 1000,
            "hbd_exchange_rate": {
                "base": {"amount": "100000", "precision": 3, "nai": "@@000000013"},
                "quote": {"amount": "100000", "precision": 3, "nai": "@@000000021"},
            },
            "new_signing_key": tt.Account("alice-new").public_key,
            "url": "http://new-url.html",
            "key": tt.Account("alice").public_key,
            "account_subsidy_budget": 797,
            "account_subsidy_decay": 347321,
        },
    ],
)
@pytest.mark.testnet()
def test_witness_set_properties(
    prepared_node: tt.InitNode, wallet: tt.Wallet, alice: WitnessAccount, props_to_serialize: dict
) -> None:
    alice.become_witness("http://url.html", tt.Asset.Test(28), 131072, 1000)

    alice.check_if_account_has_witness_role(expected_witness_role=True)
    alice.rc_manabar.update()
    trx = alice.witness_set_properties(props_to_serialize=props_to_serialize)
    alice.assert_if_rc_current_mana_was_reduced(trx)
