#!/usr/bin/python3

import time

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet( args ) as wallet:
            # setup
            creator, pool_account = make_user_for_tests(wallet)
            receiver = get_valid_hive_account_name()
            print(wallet.create_account(creator, receiver, "{}", "true"))
            wallet.transfer(creator, receiver, "10.000 TESTS", "", "true")
            wallet.transfer_to_vesting(creator, receiver, "0.001 TESTS", "true")

            rc = wallet.find_rc_accounts([receiver])['result'][0]

            assert len(rc['incoming_delegations']) == 2
            assert(rc['incoming_delegations'][0][0] == creator)
            assert(rc['incoming_delegations'][0][1]["rc_manabar"]["current_mana"] == 0)
            assert(rc['incoming_delegations'][0][1]['max_mana'] == 0)
            assert rc['incoming_delegations'][1][0] == "null"
            assert(rc['incoming_delegations'][1][1]["rc_manabar"]["current_mana"] == 0)
            assert(rc['incoming_delegations'][1][1]['max_mana'] == 0)

            result = wallet.set_slot_delegator(pool_account, receiver, 2, receiver, "true")
            rc = wallet.find_rc_accounts([receiver])['result'][0]
            assert len(rc['incoming_delegations']) == 3

            ## use up all of the available rc
            transfer_result = wallet.transfer(receiver, receiver, "0.001 TESTS", "", "true")
            while "error" not in transfer_result:
                transfer_result = wallet.transfer(receiver, receiver, "0.001 TESTS", "", "true")

            wallet.delegate_to_pool(creator, pool_account, {"symbol":"VESTS","amount": "100", "decimals": 6, "nai": "@@000000037"}, "true")
            pool = wallet.list_rc_delegation_pools(pool_account, 100, "by_name")['result'][0]
            pool_current_mana = pool['rc_pool_manabar']['current_mana']
            assert(pool['max_rc'] == 100)
            assert(pool_current_mana == 100)

            transfer_result = wallet.transfer(receiver, creator, "0.001 TESTS", "", "true")
            assert "error" in transfer_result, "Should not be able to transfer with no RC"

            res = wallet.delegate_drc_from_pool(pool_account, receiver, {"decimals": 6, "nai": "@@000000037"}, 100, "true")
            rc = wallet.find_rc_accounts([receiver])['result'][0]
            current_mana = rc['incoming_delegations'][0][1]["rc_manabar"]["current_mana"]
            assert(rc['incoming_delegations'][0][0] == receiver)
            assert(current_mana == 100)
            assert(rc['incoming_delegations'][0][1]['max_mana'] == 100)

            transfer_result = wallet.transfer(receiver, creator, "1.000 TESTS", "true")
            assert "error" not in transfer_result, "Should be able to transfer"

            rc = wallet.find_rc_accounts([receiver])['result'][0]
            assert(rc['incoming_delegations'][0][0] == "initminer")
            assert(rc['incoming_delegations'][0][1]["rc_manabar"]["current_mana"] < current_mana)
            assert(rc['incoming_delegations'][0][1]['max_mana'] == 100)

