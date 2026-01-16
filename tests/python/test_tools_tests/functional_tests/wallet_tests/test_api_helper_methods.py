from __future__ import annotations

import pytest
import test_tools as tt


@pytest.fixture
def node(request: pytest.FixtureRequest) -> tt.InitNode:
    init_node = tt.InitNode()
    init_node.config.plugin.append("condenser_api")
    shared_file_size = request.node.get_closest_marker("node_shared_file_size")
    if shared_file_size:
        init_node.config.shared_file_size = shared_file_size.args[0]

    init_node.run()

    return init_node


@pytest.fixture
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


def test_keys_import_during_account_creation(wallet: tt.Wallet) -> None:
    accounts = wallet.create_accounts(3)
    imported_private_keys = wallet.api.list_keys()

    assert all(account.public_key in imported_private_keys for account in accounts)


@pytest.mark.node_shared_file_size("16G")
def test_creation_of_huge_number_of_accounts(node: tt.InitNode, wallet: tt.Wallet) -> None:
    amount_of_accounts_to_create = 200_000

    # This is required to make possible pushing 21 trxs per block after 21'th block
    wallet.api.update_witness(
        witness_name="initminer",
        url="https://initminer.com",
        block_signing_key=tt.Account("initminer").public_key,
        props={
            "account_creation_fee": tt.Asset.Test(amount=1),
            "maximum_block_size": 2097152,
            "hbd_interest_rate": 0,
        },
    )

    tt.logger.info("Wait 42 blocks for change of account_creation_fee")
    node.wait_for_block_with_number(42)

    before = node.api.condenser.get_account_count()
    accounts_before = set(wallet.list_accounts())
    created_accounts = wallet.create_accounts(amount_of_accounts_to_create, import_keys=False)

    node.wait_number_of_blocks(20)

    after = node.api.condenser.get_account_count()
    accounts_after = set(wallet.list_accounts())
    tt.logger.info(f"{before} ->  {after}")

    assert len(created_accounts) == amount_of_accounts_to_create
    assert len(accounts_after.difference(accounts_before)) == amount_of_accounts_to_create


@pytest.mark.node_shared_file_size("16G")
def test_creation_of_huge_number_of_accounts_and_import_keys(node: tt.InitNode, wallet: tt.Wallet) -> None:
    """
    Time: 03m 52s.

    Hardware:
        MEMORY: 16GiB DIMM DDR4 Synchronous Unbuffered (Unregistered) 2133 MHz (0,5 ns)
        STORAGE: Samsung SSD 980 1TB NVMe disk
        PROCESSOR: AMD Ryzen 7 5700G with Radeon Graphics 8 core, 16 threads.
    """
    amount_of_accounts_to_create = 200_000

    # This is required to make possible pushing 21 trxs per block after 21'th block
    wallet.api.update_witness(
        witness_name="initminer",
        url="https://initminer.com",
        block_signing_key=tt.Account("initminer").public_key,
        props={
            "account_creation_fee": tt.Asset.Test(amount=1),
            "maximum_block_size": 2097152,
            "hbd_interest_rate": 0,
        },
    )

    tt.logger.info("Wait 42 blocks for change of account_creation_fee")
    node.wait_for_block_with_number(42)

    wallet.create_accounts(amount_of_accounts_to_create, import_keys=True)

    assert len(wallet.api.list_keys()) - 1 == amount_of_accounts_to_create, "Keys was not imported correctly"
