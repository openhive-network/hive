from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import create_account_with_different_keys
from python.functional.operation_tests.conftest import UpdateAccount
from wax import WaxChainOptions, create_hive_chain

if TYPE_CHECKING:
    from wax.interfaces import IHiveChainInterface


@pytest.fixture()
def alice(prepared_node: tt.InitNode, wallet: tt.Wallet) -> UpdateAccount:
    # slow down node - speeding up time caused random fails (it's not possible to do "+0h x1")
    prepared_node.restart(time_control=tt.OffsetTimeControl(offset="+1h"))
    # wallet.create_account creates account with 4 the same keys which is not wanted in this kind of tests
    create_account_with_different_keys(wallet, "alice", "initminer")
    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(50), broadcast=True)
    return UpdateAccount("alice", prepared_node, wallet)


@pytest.fixture()
def owner() -> tt.Account:
    return tt.Account("alice", secret="owner")


@pytest.fixture()
def active() -> tt.Account:
    return tt.Account("alice", secret="active")


@pytest.fixture()
def posting() -> tt.Account:
    return tt.Account("alice", secret="posting")


@pytest.fixture()
def node() -> tt.InitNode:
    init_node = tt.InitNode()
    init_node.run()
    return init_node


@pytest.fixture()
def chain(node) -> IHiveChainInterface:
    return create_hive_chain(WaxChainOptions(endpoint_url=node.http_endpoint))


@pytest.fixture()
def alice_wallet(node: tt.InitNode, posting, active, owner) -> tt.Wallet:
    wallet = tt.Wallet(attach_to=node)
    wallet.api.create_account_with_keys(
        "initminer",
        "alice",
        "",
        posting=posting.public_key,
        active=active.public_key,
        owner=owner.public_key,
        memo=tt.Account("alice", secret="memo").public_key,
    )
    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(500))

    return wallet
