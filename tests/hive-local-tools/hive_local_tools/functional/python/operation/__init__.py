from datetime import datetime
from copy import deepcopy
import test_tools as tt
from hive_local_tools.constants import filters_enum_virtual_ops, TRANSACTION_TEMPLATE


def check_if_fill_transfer_from_savings_vop_was_generated(node: tt.InitNode, memo: str) -> bool:
    payout_vops = get_virtual_operations(node, "fill_transfer_from_savings_operation")
    for vop in payout_vops:
        if vop['op']['value']['memo'] == memo:
            return True
    return False


def create_transaction_with_any_operation(wallet, operation_name, **kwargs):
    # function creates transaction manually because some operations are not added to wallet
    transaction = deepcopy(TRANSACTION_TEMPLATE)
    transaction["operations"].append([operation_name, kwargs])
    return wallet.api.sign_transaction(transaction)


def get_governance_voting_power(node: tt.InitNode, wallet: tt.Wallet, account_name: str) -> int:
    try:
        wallet.api.set_voting_proxy(account_name, "initminer")
    finally:
        return int(node.api.database.find_accounts(accounts=["initminer"])["accounts"][0]['proxied_vsf_votes'][0])


def get_hbd_balance(node: tt.InitNode, account_name: str) -> tt.Asset.Tbd:
    return tt.Asset.from_(node.api.database.find_accounts(accounts=[account_name])['accounts'][0]['hbd_balance'])


def get_hbd_savings_balance(node: tt.InitNode, account_name: str) -> tt.Asset.Tbd:
    return tt.Asset.from_(node.api.database.find_accounts(accounts=[account_name])['accounts'][0]['savings_hbd_balance'])


def get_hive_balance(node: tt.InitNode, account_name: str) -> tt.Asset.Test:
    return tt.Asset.from_(node.api.database.find_accounts(accounts=[account_name])['accounts'][0]['balance'])


def get_hive_power(node: tt.InitNode, account_name: str) -> tt.Asset.Vest:
    return tt.Asset.from_(node.api.database.find_accounts(accounts=[account_name])['accounts'][0]['vesting_shares'])


def get_current_mana(node: tt.InitNode, account_name: str) -> int:
    return node.api.rc.find_rc_accounts(accounts=[account_name])['rc_accounts'][0]['rc_manabar']['current_mana']


def get_number_of_fill_order_operations(node):
    return len(node.api.account_history.enum_virtual_ops(block_range_begin=1,
                                                         block_range_end=node.get_last_block_number() + 1,
                                                         include_reversible=True,
                                                         filter=0x000080,
                                                         group_by_block=False)["ops"])


def get_vesting_price(node: tt.InitNode) -> int:
    """
    Current exchange rate - `1` Hive to Vest conversion price.
    """
    dgpo = node.api.wallet_bridge.get_dynamic_global_properties()
    total_vesting_shares = dgpo['total_vesting_shares']["amount"]
    total_vesting_fund_hive = dgpo['total_vesting_fund_hive']["amount"]

    return int(total_vesting_shares) // int(total_vesting_fund_hive)


def convert_hive_to_vest_range(hive_amount: tt.Asset.Test, price: float, tolerance: int = 5) -> tt.Asset.Range:
    """
     Converts Hive to VEST resources at the current exchange rate provided in the `price` parameter.
    :param hive_amount: The amount of Hive resources to convert.
    :param price: The current exchange rate for the conversion.
    :param tolerance: The tolerance percent for the VEST conversion, defaults its 5%.
    :return: The equivalent amount of VEST resources after the conversion, within the specified tolerance.
    """
    vests = tt.Asset.from_({"amount": hive_amount.amount * price, "precision": 6, "nai": "@@000000037"})
    return tt.Asset.Range(vests, tolerance=tolerance)


def get_virtual_operations(node: tt.InitNode, vop: str, skip_price_stabilization: bool = True) -> list:
    """
    :param vop: name of the virtual operation,
    :param skip_price_stabilization: by default, removes from the list operations confirming vesting-stabilizing,
    :return: a list of virtual operations of the type specified in the `vop` argument.
    """
    result = node.api.account_history.enum_virtual_ops(
        filter=filters_enum_virtual_ops[vop],
        include_reversible=True,
        block_range_end=2000,
    )["ops"]

    if skip_price_stabilization and vop == "transfer_to_vesting_completed_operation":
        for vop_number, vop in enumerate(result):
            if vop['op']['value']['hive_vested'] == tt.Asset.Test(10_000_000):
                result.pop(vop_number)
    return result


def get_rc_max_mana(node: tt.InitNode, account_name: str) -> int:
    return int(node.api.rc.find_rc_accounts(accounts=[account_name])['rc_accounts'][0]["max_rc"])


def get_transaction_timestamp(node: tt.InitNode, transaction):
    return tt.Time.parse(node.api.block.get_block(block_num=transaction["block_num"])["block"]["timestamp"])


def jump_to_date(node: tt.InitNode, time_offset: datetime) -> None:
    node.restart(time_offset=tt.Time.serialize(time_offset, format_=tt.Time.TIME_OFFSET_FORMAT,))
