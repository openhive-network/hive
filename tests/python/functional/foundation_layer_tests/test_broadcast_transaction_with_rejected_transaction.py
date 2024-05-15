from __future__ import annotations

from typing import TYPE_CHECKING, Callable

import pytest

import test_tools as tt
from hive_local_tools import run_for

if TYPE_CHECKING:
    from schemas.transaction import Transaction

ACCOUNT = "alice"


def broadcast_transaction_with_condenser_api(node: tt.InitNode | tt.RemoteNode, transaction: Transaction) -> None:
    node.api.condenser.broadcast_transaction(transaction)


def broadcast_transaction_with_network_broadcast_api(
    node: tt.InitNode | tt.RemoteNode, transaction: Transaction
) -> None:
    node.api.network_broadcast.broadcast_transaction(trx=transaction)


def broadcast_transaction_with_wallet_bridge_api(node: tt.InitNode | tt.RemoteNode, transaction: Transaction) -> None:
    node.api.wallet_bridge.broadcast_transaction(transaction)


@pytest.mark.parametrize(
    "broadcast",
    [
        # broadcast_transaction_with_condenser_api,  # deprecated in new object Wallet
        broadcast_transaction_with_network_broadcast_api,
        broadcast_transaction_with_wallet_bridge_api,
    ],
)
@run_for("testnet")
def test_broadcast_transaction_with_rejected_transaction(
    node: tt.InitNode | tt.RemoteNode,
    broadcast: Callable[[tt.InitNode | tt.RemoteNode, Transaction], None],
) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", ACCOUNT, "{}")
    transaction = wallet.api.transfer(ACCOUNT, "initminer", tt.Asset.Test(100), "{}", broadcast=False)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        broadcast(node, transaction)

    assert f"Account {ACCOUNT} does not have sufficient funds for balance adjustment." in exception.value.error


@pytest.mark.parametrize(
    "api",
    [
        # "condenser",  # deprecated in new object Wallet
        "wallet_bridge",
    ],
)
@run_for("testnet")
def test_broadcast_transaction_synchronous_with_rejected_transaction(
    node: tt.InitNode | tt.RemoteNode, api: str
) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", ACCOUNT, "{}")
    transaction = wallet.api.transfer(ACCOUNT, "initminer", tt.Asset.Test(100), "{}", broadcast=False)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        getattr(node.api, api).broadcast_transaction_synchronous(transaction)

    assert f"Account {ACCOUNT} does not have sufficient funds for balance adjustment." in exception.value.error
