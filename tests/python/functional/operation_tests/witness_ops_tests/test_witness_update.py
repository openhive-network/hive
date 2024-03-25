from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import WitnessAccount


@pytest.mark.testnet()
def test_become_a_witness(prepared_node: tt.InitNode, wallet: tt.Wallet, alice: WitnessAccount) -> None:
    # test case 1.1 from https://gitlab.syncad.com/hive/hive/-/issues/634
    trx = alice.become_witness("http://url.html", tt.Asset.Test(28), 131072, 1000)
    alice.assert_if_rc_current_mana_was_reduced(trx)
    alice.check_if_account_has_witness_role(expected_witness_role=True)


@pytest.mark.testnet()
def test_resign_from_witness_role(prepared_node: tt.InitNode, wallet: tt.Wallet, alice: WitnessAccount) -> None:
    # test case 2.1 from https://gitlab.syncad.com/hive/hive/-/issues/634
    trx = alice.become_witness("http://url.html", tt.Asset.Test(28), 131072, 1000)
    alice.assert_if_rc_current_mana_was_reduced(trx)
    alice.check_if_account_has_witness_role(expected_witness_role=True)
    trx = alice.resign_from_witness_role()
    alice.assert_if_rc_current_mana_was_reduced(trx)
    alice.check_if_account_has_witness_role(expected_witness_role=False)


@pytest.mark.parametrize(
    "update_witness_properties_args",
    [
        {"new_url": "http://new-url.html"},
        {"new_maximum_block_size": 131070},
        {"new_hbd_interest_rate": 1111},
        {"new_block_signing_key": tt.Account("alice-new").public_key},
        {"new_account_creation_fee": tt.Asset.Test(31)},
        {
            "new_url": "http://new-url.html",
            "new_maximum_block_size": 131070,
            "new_hbd_interest_rate": 1111,
            "new_block_signing_key": tt.Account("alice-new").public_key,
            "new_account_creation_fee": tt.Asset.Test(31),
        },
    ],
)
@pytest.mark.testnet()
def test_update_witness_properties(
    prepared_node: tt.InitNode, wallet: tt.Wallet, alice: WitnessAccount, update_witness_properties_args: dict
) -> None:
    # test cases 3.1 - 3.5 from https://gitlab.syncad.com/hive/hive/-/issues/634
    trx = alice.become_witness("http://url.html", tt.Asset.Test(28), 131072, 1000)
    alice.assert_if_rc_current_mana_was_reduced(trx)
    alice.check_if_account_has_witness_role(expected_witness_role=True)
    trx = alice.update_witness_properties(**update_witness_properties_args)
    alice.assert_if_rc_current_mana_was_reduced(trx)
