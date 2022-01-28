import pytest

from test_tools import Wallet


@pytest.fixture
def wallet(world, request):
    init_node = world.create_init_node()

    shared_file_size = request.node.get_closest_marker('node_shared_file_size')
    if shared_file_size:
        init_node.config.shared_file_size = shared_file_size.args[0]

    init_node.run()

    return Wallet(attach_to=init_node)


def test_keys_import_during_account_creation(wallet):
    accounts = wallet.create_accounts(3)
    imported_private_keys = set(key_pair[1] for key_pair in wallet.api.list_keys())

    assert all(account.private_key in imported_private_keys for account in accounts)


@pytest.mark.node_shared_file_size('16G')
def test_creation_of_huge_number_of_accounts(wallet):
    accounts_before = set(wallet.list_accounts())
    created_accounts = wallet.create_accounts(100_000, import_keys=False)
    accounts_after = set(wallet.list_accounts())

    assert len(created_accounts) == 100_000
    assert len(accounts_after.difference(accounts_before)) == 100_000
