from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import WitnessAccount


@pytest.mark.testnet()
def test_publish_feed_from_witness_account(
    prepared_node: tt.InitNode, wallet: tt.Wallet, alice: WitnessAccount
) -> None:
    # test case 1.1 from https://gitlab.syncad.com/hive/hive/-/issues/633
    alice.become_witness("http://url.html", tt.Asset.Test(28), 131072, 1000)
    alice.check_if_account_has_witness_role(expected_witness_role=True)
    alice.rc_manabar.update()
    trx = alice.feed_publish(base=1000, quote=100)
    alice.assert_if_rc_current_mana_was_reduced(trx)
    alice.assert_if_feed_publish_operation_was_generated(trx)


@pytest.mark.testnet()
def test_publish_feed_from_non_witness_account(
    prepared_node: tt.InitNode, wallet: tt.Wallet, alice: WitnessAccount
) -> None:
    # test case 2.1 from https://gitlab.syncad.com/hive/hive/-/issues/633
    with pytest.raises(tt.exceptions.CommunicationError) as error:
        alice.feed_publish(base=1000, quote=100)
    assert "unknown key" in error.value.error, "Message other than expected."
    alice.check_if_account_has_witness_role(expected_witness_role=False)
    alice.assert_rc_current_mana_was_unchanged()
