from __future__ import annotations

import pytest

import test_tools as tt


class Account:
    def __init__(self, node: tt.InitNode, name: str):
        self.name = name
        self.node = node
        self.wallet = tt.Wallet(attach_to=node, preconfigure=False)
        self.wallet.api.set_password(self.wallet.DEFAULT_PASSWORD)
        self.wallet.api.unlock(self.wallet.DEFAULT_PASSWORD)


@pytest.fixture()
def initminer_wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def alice(node: tt.InitNode, initminer_wallet: tt.Wallet) -> Account:
    alice = Account(node, "alice")
    initminer_wallet.api.create_account_with_keys(
        "initminer",
        alice.name,
        "",
        posting=tt.Account(alice.name, secret="posting").public_key,
        active=tt.Account(alice.name, secret="active").public_key,
        owner=tt.Account(alice.name, secret="owner").public_key,
        memo=tt.Account(alice.name, secret="memo").public_key,
    )

    alice.wallet.api.import_keys(
        [
            tt.Account(alice.name, secret="posting").private_key,
            tt.Account(alice.name, secret="active").private_key,
            tt.Account(alice.name, secret="owner").private_key,
            tt.Account(alice.name, secret="memo").private_key,
        ]
    )

    initminer_wallet.api.transfer_to_vesting("initminer", alice.name, tt.Asset.Test(100))
    initminer_wallet.api.transfer("initminer", alice.name, tt.Asset.Test(100), f"transfer hive to {alice.name}.")

    return alice


@pytest.fixture()
def bob(node: tt.InitNode, initminer_wallet: tt.Wallet) -> Account:
    bob = Account(node, "bob")
    initminer_wallet.api.create_account_with_keys(
        "initminer",
        bob.name,
        "",
        posting=tt.Account(bob.name, secret="posting").public_key,
        active=tt.Account(bob.name, secret="active").public_key,
        owner=tt.Account(bob.name, secret="owner").public_key,
        memo=tt.Account(bob.name, secret="memo").public_key,
    )

    assert bob.wallet.api.list_keys() == [], "Bob's wallet has an incorrect number of keys. Expected 0"
    return bob
