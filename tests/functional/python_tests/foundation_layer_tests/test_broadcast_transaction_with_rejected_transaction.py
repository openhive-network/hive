import pytest

import test_tools as tt
from hive_local_tools import run_for

ACCOUNT = "alice"


def broadcast_transaction_with_condenser_api(node, transaction):
    node.api.condenser.broadcast_transaction(transaction)


def broadcast_transaction_with_network_broadcast_api(node, transaction):
    node.api.network_broadcast.broadcast_transaction(trx=transaction)


def broadcast_transaction_with_wallet_bridge_api(node, transaction):
    node.api.wallet_bridge.broadcast_transaction(transaction)


@pytest.mark.parametrize(
    "broadcast",
    [
        broadcast_transaction_with_condenser_api,
        broadcast_transaction_with_network_broadcast_api,
        broadcast_transaction_with_wallet_bridge_api,
    ],
)
@run_for("testnet")
def test_broadcast_transaction_with_rejected_transaction(node, broadcast):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", ACCOUNT, "{}")
    transaction = wallet.api.transfer(ACCOUNT, "initminer", tt.Asset.Test(100), "{}", broadcast=False)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        broadcast(node, transaction)

    error_response = exception.value.response
    assert (
        f"Account {ACCOUNT} does not have sufficient funds for balance adjustment."
        in error_response["error"]["message"]
    )


@pytest.mark.parametrize(
    "api",
    [
        "condenser",
        "wallet_bridge",
    ],
)
@run_for("testnet")
def test_broadcast_transaction_synchronous_with_rejected_transaction(node, api):
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account("initminer", ACCOUNT, "{}")
    transaction = wallet.api.transfer(ACCOUNT, "initminer", tt.Asset.Test(100), "{}", broadcast=False)

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        getattr(node.api, api).broadcast_transaction_synchronous(transaction)

    error_response = exception.value.response
    assert (
        f"Account {ACCOUNT} does not have sufficient funds for balance adjustment."
        in error_response["error"]["message"]
    )
