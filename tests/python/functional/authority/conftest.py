from __future__ import annotations

import pytest

import test_tools as tt


class Account:
    def __init__(self, node, name: str):
        self.name = name
        self.node = node
        self.wallet = tt.Wallet(attach_to=node)


@pytest.fixture()
def initminer_wallet(node):
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def alice(node, initminer_wallet):
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
def bob(node, initminer_wallet):
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

    return bob
