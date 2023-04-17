import test_tools as tt
from hive_local_tools.constants import filters_enum_virtual_ops


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


def get_vesting_price(node: tt.InitNode) -> int:
    """
    Current exchange rate - `1` Hive to Vest conversion price.
    """
    dgpo = node.api.wallet_bridge.get_dynamic_global_properties()
    total_vesting_shares = dgpo['total_vesting_shares']["amount"]
    total_vesting_fund_hive = dgpo['total_vesting_fund_hive']["amount"]

    return int(total_vesting_shares) // int(total_vesting_fund_hive)


def hive_to_vest_conversion_value(hive_amount: tt.Asset.Test, price: float) -> tt.Asset.Vest:
    return tt.Asset.from_(
        {
            'amount': hive_amount.amount * price,
            'precision': 6,
            'nai': '@@000000037'
        }
    )


def get_virtual_operation(node: tt.InitNode, vop: str) -> list:
    return node.api.account_history.enum_virtual_ops(
        # To stabilize operation costs while creating a test environment, the initminer performs the
        # `transfer_to_vesting` operation. This happens in the initial blocks. So for testing purposes
        # this method omits this transfer starting at block 10
        block_range_begin=10,
        block_range_end=1000,
        filter=filters_enum_virtual_ops[vop],
    )["ops"]


def get_max_mana(node: tt.InitNode, account_name: str) -> int:
    return int(node.api.rc.find_rc_accounts(accounts=[account_name])['rc_accounts'][0]["max_rc"])
