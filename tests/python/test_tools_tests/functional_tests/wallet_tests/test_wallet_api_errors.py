from __future__ import annotations

import pytest
import test_tools as tt
from beekeepy.exceptions import ErrorInResponseError

from wax.helpy import Hf26Asset as Asset


@pytest.fixture
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


def test_if_raise_when_parameters_are_bad(wallet: tt.Wallet) -> None:
    with pytest.raises(RuntimeError):
        wallet.api.create_account("surely", "bad", "arguments")


def test_if_raise_when_operation_is_invalid(wallet: tt.Wallet) -> None:
    with pytest.raises(ErrorInResponseError):
        # Operation is invalid because account "alice" doesn't exists
        wallet.api.transfer("initminer", "alice", Asset.Test(1), "memo")
