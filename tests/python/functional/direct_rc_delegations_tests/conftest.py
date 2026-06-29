from __future__ import annotations

import pytest

import test_tools as tt


def pytest_configure(config):
    config.addinivalue_line("markers", "node_shared_file_size: Shared file size of node from `node` fixture")


@pytest.fixture()
def node(request):
    init_node = tt.InitNode()
    # The actual HF time does not matter as long as it's in the past

    shared_file_size = request.node.get_closest_marker("node_shared_file_size")
    if shared_file_size:
        init_node.config.shared_file_size = shared_file_size.args[0]

    init_node.run(
        environment_variables={"HIVE_HF26_TIME": "1598558400"},
        timeout=60.0,
        max_retries=3,
    )
    # Lower account_creation_fee to its minimum so the RC min_delegation rounds down to 0 (if we left default median of 0.030
    # which is established by activation of HF20, which multiplies default minimum fee by 30, min_delegation would be ~3602,
    # which rejects the small delegations these tests rely on; that is why we have to reset the minimum and wait for activation
    # of the schedule with such setting).
    fee_wallet = tt.Wallet(attach_to=init_node)
    fee_wallet.api.update_witness(
        "initminer",
        "https://initminer.com",
        tt.Account("initminer").public_key,
        {"account_creation_fee": tt.Asset.Test(0.001), "maximum_block_size": 65536, "hbd_interest_rate": 0},
    )
    init_node.wait_for_block_with_number(43)
    fee_wallet.close()
    return init_node


@pytest.fixture()
def wallet(node):
    return tt.Wallet(attach_to=node)
