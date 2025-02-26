from __future__ import annotations

from copy import deepcopy
from typing import Any

import pytest
from beekeepy.exceptions import CommunicationError

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import TRANSACTION_TEMPLATE

ALICE_MASTER_PASSWORD = "Alice has a cat"


@run_for("testnet")
@pytest.mark.parametrize("broadcast_way", ["api", "wallet"])
@pytest.mark.parametrize("memo_type", ["private_key", "master_password", "extended_private_key"])
@pytest.mark.parametrize("role", ["owner", "active", "posting", "memo"])
@pytest.mark.parametrize(
    "operation", ["transfer", "recurrent_transfer", "transfer_to_savings", "transfer_from_savings"]
)
def test_handling_sensitive_data_in_the_memo_field(
    node: tt.InitNode | tt.RemoteNode,
    wallet: tt.OldWallet,
    broadcast_way: str,
    memo_type: str,
    operation: str,
    role: str,
) -> None:
    keys = {
        "owner": tt.PublicKey("alice", secret="owner"),
        "active": tt.PublicKey("alice", secret="active"),
        "posting": tt.PublicKey("alice", secret="posting"),
        "memo": tt.PublicKey("alice", secret="memo"),
    }

    private_key = tt.PrivateKey("alice", secret="active")
    wallet.api.import_key(private_key)

    if memo_type != "private_key":
        public_key, private_key = wallet.api.get_private_key_from_password("alice", role, ALICE_MASTER_PASSWORD)
        wallet.api.import_key(private_key)
        if memo_type == "extended_private_key":
            private_key = get_extended_private_key(role)
        keys.update({role: public_key})
    else:
        private_key = tt.PrivateKey("alice", secret=f"{role}")
        wallet.api.import_key(private_key)

    wallet.api.create_account_with_keys("initminer", "alice", "{}", **keys)
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(100), "{}")
    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(10))
    if operation == "transfer_from_savings":
        wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "{}")

    # To sign the tested operations, using the `active` key is adequate
    wallet.api.use_authority("active", "alice")

    memo_message = get_memo_message(memo_type, role, private_key)

    error_message = "#"
    with pytest.raises(CommunicationError) as error:  # noqa: PT012
        match broadcast_way:
            case "api":
                error_message = f"Detected private {role} key in memo field. You should change your {role} key"
                broadcast_transaction_by_api(node, wallet, operation, memo_message)
            case "wallet":
                error_message = f"Detected private {role} key in memo field. Cancelling transaction."
                broadcast_transaction_by_wallet(wallet, operation, memo_message)

    assert error_message in error.value.get_response_error_messages()[0]


@run_for("testnet")
@pytest.mark.parametrize(
    "operation", ["transfer", "recurrent_transfer", "transfer_to_savings", "transfer_from_savings"]
)
def test_handle_by_wallet_additional_private_key_in_memo_field(
    node: tt.InitNode | tt.RemoteNode,
    wallet: tt.OldWallet,
    operation: str,
) -> None:
    extra_private_key = tt.PrivateKey("alice", secret="extra_key")
    wallet.api.import_key(extra_private_key)

    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    with pytest.raises(CommunicationError) as error:
        broadcast_transaction_by_wallet(wallet, operation, memo=extra_private_key)

    assert "Detected imported private key in memo field. Cancelling transaction." in str(error.value)


def broadcast_transaction_by_api(
    node: tt.InitNode | tt.RemoteNode,
    wallet: tt.OldWallet,
    op: dict[str, Any],
    memo_type: str,
) -> None:
    transaction = deepcopy(TRANSACTION_TEMPLATE)
    transaction["operations"] = generate_operation(op, memo_type)
    signed_transaction = wallet.api.sign_transaction(tx=transaction, broadcast=False)
    node.api.network_broadcast.broadcast_transaction(trx=signed_transaction)


def broadcast_transaction_by_wallet(wallet: tt.OldWallet, operation_type: str, memo: str) -> None:
    match operation_type:
        case "transfer":
            wallet.api.transfer("alice", "initminer", tt.Asset.Test(1), memo)
        case "recurrent_transfer":
            wallet.api.recurrent_transfer("alice", "initminer", tt.Asset.Test(1), memo, 24, 4)
        case "transfer_to_savings":
            wallet.api.transfer_to_savings("alice", "alice", tt.Asset.Test(1), memo)
        case "transfer_from_savings":
            wallet.api.transfer_from_savings("alice", 1, "alice", tt.Asset.Test(1), memo)


def generate_operation(operation_type: str, memo: str) -> list[Any]:
    match operation_type:
        case "transfer":
            return [
                [
                    "transfer",
                    {
                        "amount": "1.000 TESTS",
                        "from": "alice",
                        "memo": memo,
                        "to": "initminer",
                    },
                ]
            ]
        case "recurrent_transfer":
            return [
                [
                    "recurrent_transfer",
                    {
                        "amount": "1.000 TESTS",
                        "executions": 4,
                        "extensions": [],
                        "from": "alice",
                        "memo": memo,
                        "recurrence": 24,
                        "to": "initminer",
                    },
                ]
            ]
        case "transfer_to_savings":
            return [
                [
                    "transfer_to_savings",
                    {
                        "amount": "1.000 TESTS",
                        "from": "alice",
                        "memo": memo,
                        "to": "alice",
                    },
                ]
            ]
        case "transfer_from_savings":
            return [
                [
                    "transfer_from_savings",
                    {
                        "amount": "1.000 TESTS",
                        "from": "alice",
                        "memo": memo,
                        "request_id": 1,
                        "to": "alice",
                    },
                ]
            ]


def get_extended_private_key(role: str) -> str:
    """
    Generating extended private keys is not possible using Python test-tools. The hardcoded keys in this method were
    generated using methods from C++ binaries.
    """
    extended_keys = {
        "owner": "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzFAaNn8UzQz9CLFiVxf9zN8LT5Ejbyad4Vfk9MW2xT6wN4bidNpv",
        "posting": "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzF9jnVv5vsVMCrhx856pbtEccAf7fWqP2hEScPataqXt6cCxncHe",
        "active": "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzFACq4N8gfFVGf6aFxqvewTdT7p5FxamjRh4J6M1g9excwaukLXx",
        "memo": "xprv9s21ZrQH143K24Mfq5zL5MhWK9hUhhGbd45hLXo2Pq2oqzMMo63oStZzF9CmKDncwk62M6ZmZjxwZqu1HmH2USLRQNNhnhqEpy5FLBjScLL",
    }

    return extended_keys[role]


def get_memo_message(memo_type: str, role: str, private_key: str) -> str:
    match memo_type:
        case "master_password":
            return ALICE_MASTER_PASSWORD
        case "private_key":
            return private_key
        case "extended_private_key":
            return get_extended_private_key(role)
