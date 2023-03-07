from pathlib import Path
import pytest

import test_tools as tt
from hive_local_tools.functional.python.hf28.constants import VOTER_ACCOUNT, PROXY_ACCOUNT


@pytest.fixture
def prepare_environment(node):
    node.restart(time_offset="+0 x5")
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account(VOTER_ACCOUNT, vests=tt.Asset.Test(10))
    wallet.create_account(PROXY_ACCOUNT)

    return node, wallet


@pytest.fixture
def prepare_environment_on_hf_27(node):
    # run on a node with a date earlier than the start date of hardfork 28 (february 8, 2023 1:00:00 am)
    node = tt.WitnessNode(witnesses=[f"witness{i}-alpha" for i in range(0, 20)])


    block_log_directory = Path(__file__).parent / "block_log"
    with open(block_log_directory / "timestamp", encoding="utf-8") as file:
        absolute_start_time = tt.Time.parse(file.read())

    absolute_start_time -= tt.Time.seconds(5)
    time_offset = tt.Time.serialize(absolute_start_time, format_=tt.Time.TIME_OFFSET_FORMAT)
    node.run(
        replay_from=block_log_directory /"block_log",
        time_offset=time_offset,
    )

    wallet = tt.Wallet(attach_to=node)
    wallet.create_account(VOTER_ACCOUNT, vests=tt.Asset.Test(10))
    wallet.create_account(PROXY_ACCOUNT)

    assert node.api.condenser.get_hardfork_version() == "1.27.0"
    return node, wallet
