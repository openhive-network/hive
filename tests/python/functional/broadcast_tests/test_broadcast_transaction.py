# ruff: noqa

from __future__ import annotations

from time import sleep

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools import run_for


@pytest.mark.parametrize("max_block_age_value", [-1, 2000, -2000, 20000, -20000])
@run_for("testnet", enable_plugins=["account_history_api"])
def test_broadcast_account_creating_with_correct_values(node, wallet, max_block_age_value):
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    node.api.network_broadcast.broadcast_transaction(trx=transaction, max_block_age=max_block_age_value)
    node.wait_number_of_blocks(1)

    assert transaction != node.api.account_history.get_transaction(
        id=transaction["transaction_id"], include_reversible=True
    )
    assert wallet.api.get_account("alice")


@run_for("testnet", enable_plugins=["account_history_api"])
def test_broadcast_account_creating_with_incorrect_value(node, wallet):
    with pytest.raises(ErrorInResponseError):  # noqa: PT012
        for i in range(6):
            transaction = wallet.api.create_account("initminer", f"alice{i}", "{}", broadcast=False)
            node.api.network_broadcast.broadcast_transaction(trx=transaction, max_block_age=0)
            sleep(1)
    node.wait_number_of_blocks(1)

    with pytest.raises(ErrorInResponseError):
        node.api.account_history.get_transaction(id=transaction["transaction_id"], include_reversible=True)

    with pytest.raises(tt.exceptions.AccountNotExistError):
        wallet.api.get_account(f"alice{i}")


@run_for("testnet", enable_plugins=["account_history_api"])
def test_broadcast_same_transaction_twice(node, wallet):
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    node.api.network_broadcast.broadcast_transaction(trx=transaction)
    node.wait_number_of_blocks(1)

    accounts = node.api.database.find_accounts(accounts=["alice"]).accounts
    assert len(accounts) == 1
    assert accounts[0].name == "alice"

    with pytest.raises(ErrorInResponseError):
        node.api.network_broadcast.broadcast_transaction(trx=transaction)


@pytest.mark.parametrize(
    "way_of_broadcasting",
    [
        # "node.api.condenser.broadcast_transaction(transaction)",  # deprecated in new object Wallet as it only supports hf26 which is not accepted by condenser_api
        "node.api.network_broadcast.broadcast_transaction(trx=transaction)",
        "node.api.wallet_bridge.broadcast_transaction(transaction)",
    ],
)
@run_for("testnet")
def test_broadcasting_manually_signed_transaction(node, wallet, way_of_broadcasting):
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    eval(way_of_broadcasting)
    assert "alice" in wallet.list_accounts()


@run_for("testnet")
def test_broadcasting_manually_signed_transaction_with_condenser(node, wallet):
    transaction = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    with pytest.raises(ErrorInResponseError):
        node.api.condenser.broadcast_transaction(transaction)
