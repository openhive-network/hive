import sys
sys.path.append("../../")
import hive_utils

import logging

LOG_LEVEL = logging.INFO

MODULE_NAME = "RC-Tests.RC-Utils"
logger = logging.getLogger(MODULE_NAME)
logger.setLevel(LOG_LEVEL)

# create_account "initminer" "pychol" "" true
def create_accounts(node, creator, accounts):
    for account in accounts:
        logger.info("Creating account: {}".format(account['name']))
        node.create_account(account['name'], 
            owner_key=account['public_key'], 
            active_key=account['public_key'], 
            posting_key=account['public_key'],
            memo_key=account['public_key'],
            store_keys = False,
            creator=creator,
            asset='TESTS'
        )
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


def transfer_to_vesting(node, from_account, account, amount, asset):
    from beem.account import Account
    logger.info("Transfer to vesting from {} to {} amount {} {}".format(from_account, account, amount, asset))
    acc = Account(from_account, hive_instance=node)
    acc.transfer_to_vesting(amount, to=account, asset=asset)
    hive_utils.common.wait_n_blocks(node.rpc.url, 5)


# transfer initminer pychol "399.000 TESTS" "initial transfer" true
# transfer initminer pychol "398.000 TBD" "initial transfer" true
def transfer_assets_to_accounts(node, from_account, accounts, amount, asset, wif=None):
    from beem.account import Account
    for acnt in accounts:
        logger.info("Transfer from {} to {} amount {} {}".format(from_account, 
            acnt['name'], amount, asset)
        )
        acc = Account(from_account, hive_instance=node)
        acc.transfer(acnt['name'], amount, asset, memo = "initial transfer")
    if wif is not None:
        hive_utils.debug_generate_blocks(node.rpc.url, wif, 5)
    else:
        hive_utils.common.wait_n_blocks(node.rpc.url, 5)

