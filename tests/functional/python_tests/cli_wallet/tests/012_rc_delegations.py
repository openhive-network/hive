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

            assert len(rc['delegation_slots']) == 3
            assert(rc['delegation_slots'][0]['delegator'] == creator)
            assert(rc['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][0]['max_mana'] == 0)

            assert(rc['delegation_slots'][1]['delegator'] == "")
            assert(rc['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][1]['max_mana'] == 0)

            assert(rc['delegation_slots'][2]['delegator'] == "")
            assert(rc['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][2]['max_mana'] == 0)

            wallet.set_slot_delegator(pool_account, receiver, 1, receiver, "true")
            rc = wallet.find_rc_accounts([receiver])['result'][0]

            assert len(rc['delegation_slots']) == 3
            assert(rc['delegation_slots'][0]['delegator'] == creator)
            assert(rc['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][0]['max_mana'] == 0)

            assert(rc['delegation_slots'][1]['delegator'] == pool_account)
            assert(rc['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][1]['max_mana'] == 0)

            assert(rc['delegation_slots'][2]['delegator'] == "")
            assert(rc['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][2]['max_mana'] == 0)

            ## use up all of the available rc
            wallet.post_comment(receiver, "lorem", "", "ipsum", "Lorem Ipsum", "body", "{}", "true")
            rc = wallet.find_rc_accounts([receiver])['result'][0]
            transfer_result = wallet.transfer(receiver, receiver, "0.001 TESTS", "", "true")
            while "error" not in transfer_result:
                transfer_result = wallet.transfer(receiver, receiver, "0.001 TESTS", "", "true")

            assert "Please wait to transact" in transfer_result['error']['message']
            
            res = wallet.delegate_to_pool(creator, pool_account, {"symbol":"VESTS","amount": "100", "precision": 6, "nai": "@@000000037"}, "true")
            assert "error" not in res, "failed to delegate to the pool"
            pool = wallet.list_rc_delegation_pools(pool_account, 100, "by_name")['result'][0]
            pool_current_mana = pool['rc_pool_manabar']['current_mana']
            assert(pool['max_rc'] == 100)
            assert(pool_current_mana == 100)

            transfer_result = wallet.transfer(receiver, creator, "0.001 TESTS", "", "true")
            assert "error" in transfer_result, "Should not be able to transfer with no RC"

            res = wallet.delegate_drc_from_pool(pool_account, receiver, {"precision": 6, "nai": "@@000000037"}, 100, "true")
            assert "error" not in res, "failed to delegate to the pool"
            rc = wallet.find_rc_accounts([receiver])['result'][0]
            current_mana = rc['delegation_slots'][1]["rc_manabar"]["current_mana"]

            assert len(rc['delegation_slots']) == 3
            assert(rc['delegation_slots'][0]['delegator'] == creator)
            assert(rc['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][0]['max_mana'] == 0)

            assert(rc['delegation_slots'][1]['delegator'] == pool_account)
            assert(rc['delegation_slots'][1]["rc_manabar"]["current_mana"] == 100)
            assert(rc['delegation_slots'][1]['max_mana'] == 100)

            assert(rc['delegation_slots'][2]['delegator'] == "")
            assert(rc['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][2]['max_mana'] == 0)

            transfer_result = wallet.transfer(receiver, creator, "1.000 TESTS", "", "true")
            assert "error" not in transfer_result, "Should be able to transfer"

            rc = wallet.find_rc_accounts([receiver])['result'][0]
            assert len(rc['delegation_slots']) == 3
            assert(rc['delegation_slots'][0]['delegator'] == creator)
            assert(rc['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][0]['max_mana'] == 0)

            assert(rc['delegation_slots'][1]['delegator'] == pool_account)
            assert rc['delegation_slots'][1]["rc_manabar"]["current_mana"] < current_mana, "RC was not deducted from the pool"
            assert(rc['delegation_slots'][1]['max_mana'] == 100)

            assert(rc['delegation_slots'][2]['delegator'] == "")
            assert(rc['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
            assert(rc['delegation_slots'][2]['max_mana'] == 0)

