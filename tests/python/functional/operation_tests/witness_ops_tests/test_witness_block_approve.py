from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import WitnessAccount


@pytest.mark.testnet()
@pytest.mark.xfail
def test_try_to_sign_witness_block_approve_operation_by_non_witness(
    prepared_node: tt.InitNode, wallet: tt.Wallet, alice: WitnessAccount
) -> None:
    # test case 1.1 from https://gitlab.syncad.com/hive/hive/-/issues/645
    alice.check_if_account_has_witness_role(expected_witness_role=False)

    with pytest.raises(ErrorInResponseError) as error:
        alice.witness_block_approve(block_id="100")
    assert "Missing Witness Authority" in error.value.error, "Error message other than expected."
    alice.assert_rc_current_mana_was_unchanged()


@pytest.mark.testnet()
@pytest.mark.xfail
def test_try_to_sign_witness_block_approve_operation_by_non_witness_authority(
    prepared_node: tt.InitNode, wallet: tt.Wallet, alice: WitnessAccount
) -> None:
    # test case 1.2 from https://gitlab.syncad.com/hive/hive/-/issues/645
    alice.become_witness("http://url.html", tt.Asset.Test(28), 131072, 1000)
    alice.check_if_account_has_witness_role(expected_witness_role=True)
    alice.rc_manabar.update()
    # alice's block signing key have to be changed for using different authority (all alice's keys rn are the same)
    alice.update_witness_properties(new_block_signing_key=tt.Account("random-name").public_key)
    alice.rc_manabar.update()
    wallet.api.use_authority("active", "alice")
    with pytest.raises(ErrorInResponseError) as error:
        alice.witness_block_approve(block_id="100")
    assert "Missing Witness Authority" in error.value.error, "Error message other than expected."
    alice.assert_rc_current_mana_was_unchanged()
