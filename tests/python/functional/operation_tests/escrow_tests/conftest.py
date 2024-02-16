from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation.escrow import EscrowTransfer
from python.functional.operation_tests.conftest import EscrowAccount

if TYPE_CHECKING:
    from _pytest.fixtures import SubRequest


@pytest.fixture()
def create_account_object() -> type[EscrowAccount]:
    return EscrowAccount


@pytest.fixture()
def prepare_escrow(
    request: SubRequest,
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    alice: EscrowAccount,
    bob: EscrowAccount,
    carol: EscrowAccount,
) -> EscrowTransfer:
    escrow = EscrowTransfer(prepared_node, wallet, alice, bob, carol)
    params = request.node.callspec.params  # get params from parametrize to create escrow with appropriate amounts
    escrow.create(
        params["hbd_amount"],
        params["hive_amount"],
        tt.Asset.Tbd(5),
        tt.Time.from_now(weeks=1),
        tt.Time.from_now(weeks=3),
    )
    return escrow
