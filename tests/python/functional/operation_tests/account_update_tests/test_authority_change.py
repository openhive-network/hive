from __future__ import annotations

import pytest

import test_tools as tt

from beekeepy._exceptions.overseer import ErrorInResponseError
from schemas.transaction import Transaction
from wax.complex_operations.account_update import AccountAuthorityUpdateOperation


@pytest.mark.asyncio
async def test_change_owner_authority_sign_with_owner_key(node, alice_wallet, chain, owner) -> None:
    trx = await chain.create_transaction()
    account_update = await AccountAuthorityUpdateOperation.create_for(chain, "alice")

    new_owner = tt.Account("alice", secret="new-owner")
    alice_wallet.api.import_key(owner.private_key)

    assert owner.public_key in alice_wallet.api.list_keys() and len(alice_wallet.api.list_keys()) == 2

    owner_role = account_update.roles.owner
    owner_role.replace(owner.public_key, 1, new_owner.public_key)

    trx.push_operation(account_update)
    alice_wallet.api.sign_transaction(Transaction.parse_raw(trx.to_api_json()), broadcast=True)

    response = node.api.database.find_accounts(accounts=["alice"]).accounts
    assert response[0].owner.key_auths[0][0] == new_owner.public_key


@pytest.mark.asyncio
async def test_change_active_authority_sign_with_active_key(node, alice_wallet, chain, active) -> None:
    trx = await chain.create_transaction()
    account_update = await AccountAuthorityUpdateOperation.create_for(chain, "alice")

    new_active = tt.Account("alice", secret="new-active")
    alice_wallet.api.import_key(active.private_key)
    assert active.public_key in alice_wallet.api.list_keys() and len(alice_wallet.api.list_keys()) == 2

    active_role = account_update.roles.active
    active_role.replace(active.public_key, 1, new_active.public_key)

    trx.push_operation(account_update)
    alice_wallet.api.sign_transaction(Transaction.parse_raw(trx.to_api_json()), broadcast=True)

    response = node.api.database.find_accounts(accounts=["alice"]).accounts
    assert response[0].active.key_auths[0][0] == new_active.public_key


@pytest.mark.asyncio
async def test_change_active_and_owner_authority_sign_with_owner_key(node, alice_wallet, chain, owner, active) -> None:
    trx = await chain.create_transaction()
    account_update = await AccountAuthorityUpdateOperation.create_for(chain, "alice")

    alice_wallet.api.import_key(owner.private_key)
    assert owner.public_key in alice_wallet.api.list_keys() and len(alice_wallet.api.list_keys()) == 2

    new_owner = tt.Account("alice", secret="new-owner")
    owner_role = account_update.roles.owner
    owner_role.replace(owner.public_key, 1, new_owner.public_key)

    new_active = tt.Account("alice", secret="new-active")
    active_role = account_update.roles.active
    active_role.replace(active.public_key, 1, new_active.public_key)

    trx.push_operation(account_update)
    alice_wallet.api.sign_transaction(Transaction.parse_raw(trx.to_api_json()), broadcast=True)

    response = node.api.database.find_accounts(accounts=["alice"]).accounts
    assert response[0].owner.key_auths[0][0] == new_owner.public_key
    assert response[0].active.key_auths[0][0] == new_active.public_key


@pytest.mark.asyncio
async def test_change_active_authority_sign_with_owner_key_negative(node, alice_wallet, chain, owner, active) -> None:
    trx = await chain.create_transaction()
    account_update = await AccountAuthorityUpdateOperation.create_for(chain, "alice")

    alice_wallet.api.import_key(owner.private_key)
    assert owner.public_key in alice_wallet.api.list_keys() and len(alice_wallet.api.list_keys()) == 2

    new_active = tt.Account("alice", secret="new-active")
    active_role = account_update.roles.active
    active_role.replace(active.public_key, 1, new_active.public_key)
    trx.push_operation(account_update)

    with pytest.raises(ErrorInResponseError) as e:
        alice_wallet.api.sign_transaction(Transaction.parse_raw(trx.to_api_json()), broadcast=True)
    assert "missing required active authority" in e.value.error


@pytest.mark.asyncio
async def test_modify_single_key_in_account_with_identical_keys(node, alice_wallet, chain) -> None:
    universal_key = tt.Account("bob", secret="bob")
    new_posting = tt.Account("bob", secret="new-posting")

    alice_wallet.api.create_account_with_keys(
        "initminer",
        "bob",
        "",
        posting=universal_key.public_key,
        active=universal_key.public_key,
        owner=universal_key.public_key,
        memo=universal_key.public_key,
    )
    alice_wallet.api.transfer_to_vesting("initminer", "bob", tt.Asset.Test(500))

    trx = await chain.create_transaction()
    account_update = await AccountAuthorityUpdateOperation.create_for(chain, "bob")

    alice_wallet.api.import_key(universal_key.private_key)
    assert universal_key.public_key in alice_wallet.api.list_keys() and len(alice_wallet.api.list_keys()) == 2

    posting_role = account_update.roles.posting
    posting_role.replace(universal_key.public_key, 1, new_posting.public_key)

    trx.push_operation(account_update)
    alice_wallet.api.sign_transaction(Transaction.parse_raw(trx.to_api_json()), broadcast=True)

    response = node.api.database.find_accounts(accounts=["bob"]).accounts
    assert response[0].posting.key_auths[0][0] == new_posting.public_key
