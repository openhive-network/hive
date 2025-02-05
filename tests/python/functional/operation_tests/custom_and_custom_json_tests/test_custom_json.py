from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools.functional.python.operation.custom_and_custom_json import CustomJson

if TYPE_CHECKING:
    from hive_local_tools.functional.python.operation import Account
from hive_local_tools.constants import TRANSACTION_TEMPLATE


@pytest.mark.parametrize(
    ("authority_types", "sign_transaction_authors", "required_auths", "required_posting_auths"),
    [
        # Test case 1.1 from https://gitlab.syncad.com/hive/hive/-/issues/632
        (["posting"], ["alice"], [], ["alice"]),
        # Test case 1.2 from https://gitlab.syncad.com/hive/hive/-/issues/632
        (["posting", "posting"], ["alice", "bob"], [], ["alice", "bob"]),
        # Test case 1.3 from https://gitlab.syncad.com/hive/hive/-/issues/632
        (["active"], ["alice"], ["alice"], []),
        # Test case 1.4 from https://gitlab.syncad.com/hive/hive/-/issues/632
        (["active", "active"], ["alice", "bob"], ["alice", "bob"], []),
    ],
)
@pytest.mark.testnet()
def test_correct_custom_json(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_alice_and_bob: tuple[Account, Account],
    authority_types: list[str],
    sign_transaction_authors: list[str],
    required_auths: list[str],
    required_posting_auths: list[str],
) -> None:
    alice, bob = create_alice_and_bob
    custom_json = CustomJson(prepared_node, wallet)
    json = '{"this": "is", "test": "json"}'
    trx = custom_json.generate_transaction(required_auths, required_posting_auths, 1, json)
    for signature_number in range(len(sign_transaction_authors)):
        broadcast = signature_number == len(sign_transaction_authors) - 1
        wallet.api.use_authority(authority_types[signature_number], sign_transaction_authors[signature_number])
        broadcasted = custom_json.sign_transaction(trx, broadcast=broadcast)
    alice.check_if_current_rc_mana_was_reduced(broadcasted)


@pytest.mark.parametrize(
    ("authority_type", "required_auths", "required_posting_auths"),
    [
        # Test case 2.1 from https://gitlab.syncad.com/hive/hive/-/issues/632
        ("posting", [], ["alice"]),
        # Test case 2.2 from https://gitlab.syncad.com/hive/hive/-/issues/632
        ("active", ["alice"], []),
    ],
)
@pytest.mark.testnet()
def test_incorrect_custom_json(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_alice_and_bob: tuple[Account, Account],
    authority_type: str,
    required_auths: list[str],
    required_posting_auths: list[str],
) -> None:
    alice, bob = create_alice_and_bob
    incorrect_json = "{this is incorrect json}"
    # inserting incorrect json to transaction requires creating it manually (otherwise schemas would throw error without even trying to broadcast it)
    trx = TRANSACTION_TEMPLATE.copy()
    operation = [
        "custom_json",
        {
            "id": "1",
            "json": incorrect_json,
            "required_auths": required_auths,
            "required_posting_auths": required_posting_auths,
        },
    ]
    trx["operations"].append(operation)

    wallet = tt.OldWallet(attach_to=prepared_node)
    wallet.api.use_authority(authority_type, alice.name)
    signed_trx = wallet.api.sign_transaction(trx, broadcast=False)
    with pytest.raises(ErrorInResponseError):
        prepared_node.api.network_broadcast.broadcast_transaction(trx=signed_trx)
    alice.check_if_rc_mana_was_unchanged()


@pytest.mark.parametrize(
    ("required_auths", "required_posting_auths"),
    [
        # Test case 2.3 from https://gitlab.syncad.com/hive/hive/-/issues/632
        (["alice"], ["alice"]),
        # Test case 2.4 from https://gitlab.syncad.com/hive/hive/-/issues/632
        (["alice"], ["bob"]),
    ],
)
@pytest.mark.testnet()
def test_correct_custom_json_with_mixed_posting_and_active_auth(
    prepared_node: tt.InitNode,
    wallet: tt.Wallet,
    create_alice_and_bob: tuple[Account, Account],
    required_auths: list[str],
    required_posting_auths: list[str],
) -> None:
    alice, bob = create_alice_and_bob
    custom_json = CustomJson(prepared_node, wallet)
    json = '{"this": "is", "test": "json"}'
    with pytest.raises(RuntimeError):  # noqa: PT012
        trx = custom_json.generate_transaction(required_auths, required_posting_auths, 1, json)
        custom_json.sign_transaction(trx, broadcast=True)
    alice.check_if_rc_mana_was_unchanged()
    bob.check_if_rc_mana_was_unchanged()
