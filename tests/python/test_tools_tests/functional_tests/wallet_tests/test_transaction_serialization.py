from __future__ import annotations

from typing import Literal

import pytest
import test_tools as tt

from wax.helpy import Hf26Asset as Asset


@pytest.mark.parametrize("transaction_serialization", ["legacy", "hf26"])
def test_transaction_serialization_getter(transaction_serialization: Literal["legacy", "hf26"]) -> None:
    wallet = tt.OldWallet(additional_arguments=[f"--transaction-serialization={transaction_serialization}"])
    assert wallet.transaction_serialization == transaction_serialization


def test_default_serialization(node: tt.InitNode) -> None:
    wallet = tt.OldWallet(attach_to=node)

    assert wallet.transaction_serialization == "legacy"

    # Check if example transaction really is legacy
    transaction = wallet.api.transfer("initminer", "alice", Asset.Test(3), "memo", broadcast=False)
    assert isinstance(transaction["operations"][0], list)
    assert transaction["operations"][0][1]["amount"] == Asset.Test(3).as_legacy()
