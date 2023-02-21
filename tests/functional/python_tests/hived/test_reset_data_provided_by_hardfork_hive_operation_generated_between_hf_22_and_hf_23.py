import operator
import time
from typing import Dict, List, Union

import pytest
import test_tools as tt
from hive_local_tools import run_for


@pytest.fixture()
def node() -> tt.InitNode:
    node = tt.InitNode()
    TIME_OF_HF_23 = "2020-03-20T14:00:00"
    # run with a date earlier than the start date of hardfork 23
    node.run(
        time_offset=tt.Time.serialize(
            tt.Time.parse(TIME_OF_HF_23) - tt.Time.seconds(10),
            format_=tt.Time.TIME_OFFSET_FORMAT,
        )
    )

    assert node.api.condenser.get_hardfork_version() == "0.22.0"
    return node


@pytest.mark.skip(reason="Remove this marker after fixing source code")
@run_for("testnet")
def test_reset_data_provided_by_hardfork_hive_operation_generated_between_hf_22_and_hf_23(node: tt.InitNode):
    """
    The test checks whether the virtual operation "hardfork_hive_operation" (generated on hardfork 23) contains
    correct data related to "air drop" HIVE.
    """
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("goku1", hives=tt.Asset.Test(50) , hbds=tt.Asset.Tbd(50), vests=tt.Asset.Test(50))
    wallet.create_account("steem", hives=tt.Asset.Test(100) , hbds=tt.Asset.Tbd(100) , vests=tt.Asset.Test(100))

    with wallet.in_single_transaction():
        wallet.api.delegate_vesting_shares("goku1", "steem", tt.Asset.Vest(5))
        wallet.api.delegate_vesting_shares("steem", "goku1", tt.Asset.Vest(10))

    assert get_hardfork_version(node) == "0.22.0"
    assert_account_resources(node, "goku1", operator.gt)
    assert_account_resources(node, "steem", operator.gt)

    __wait_for_hardfork_23_application(node)
    assert get_hardfork_version(node) == "0.23.0"

    assert_account_resources(node, "goku1", operator.eq)
    assert_account_resources(node, "steem", operator.eq)

    assert_cleared_resources_in_hardfork_hive_operation(node, "goku1")
    assert_cleared_resources_in_hardfork_hive_operation(node, "steem")


def get_hardfork_version(node: tt.InitNode) -> str:
    return node.api.wallet_bridge.get_hardfork_version()


def __wait_for_hardfork_23_application(node: tt.InitNode, timeout: int=60) -> None:
    tt.logger.info("Waiting for application hardfork 23")
    timeout = time.time() + timeout
    while True:
        if get_hardfork_version(node) == "0.23.0":
            break
        if time.time() > timeout:
            raise TimeoutError("hardfork 23 was not applied within expected time")


def assert_account_resources(node: tt.InitNode, account_name: str, relate: Union[operator.eq, operator.gt]) -> None:
    """
    Depending on the given operator (param: relate), checks whether there are funds on the account (operator: gt)
    or the account does not have funds (operator: eq).
    """
    for value in list(__get_resources_from_account(node, account_name).values()):
        asset = tt.Asset.from_(value)
        if isinstance(asset, tt.Asset.Test):
            assert relate(asset.as_nai(), tt.Asset.Test(0))
        if isinstance(asset, tt.Asset.Vest):
            assert relate(asset.as_nai(), tt.Asset.Vest(0))
        if isinstance(asset, tt.Asset.Tbd):
            assert relate(asset.as_nai(), tt.Asset.Tbd(0))


def __get_resources_from_account(node: tt.InitNode, account_name: str) -> Dict:
    account = node.api.wallet_bridge.get_account(account_name)
    return {
        "balance": account["balance"],
        "hbd_balance": account["hbd_balance"],
        "vesting_shares": account["vesting_shares"],
    }


def assert_cleared_resources_in_hardfork_hive_operation(node: tt.InitNode, account_name: str) -> None:
    hardfork_hive_virtual_operations: List = [
        op[1]["op"]["value"]
        for op in node.api.account_history.get_account_history(
            account=account_name,
            start=-1,
            limit=2,
            include_reversible=True,
            operation_filter_high=528,
        )["history"]
    ]

    asset_keys = list(tt.Asset.Hbd(1.0).as_nai().keys())
    for op in hardfork_hive_virtual_operations:
        for value in op.values():
            if isinstance(value, dict):
                if list(value.keys()) == asset_keys:
                    asset = tt.Asset.from_(value)
                    if isinstance(asset, tt.Asset.Test):
                        assert asset > tt.Asset.Test(0)
                    if isinstance(asset, tt.Asset.Vest):
                        assert asset > tt.Asset.Vest(0)
                    if isinstance(asset, tt.Asset.Tbd):
                        assert asset > tt.Asset.Tbd(0)
