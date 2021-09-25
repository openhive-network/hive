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
            creator, delegator = make_user_for_tests(wallet)
            receiver = get_valid_hive_account_name()
            print(wallet.create_account(creator, receiver, "{}", "true"))
            wallet.transfer(creator, receiver, "10.000 TESTS", "", "true")
            wallet.transfer_to_vesting(creator, delegator, "0.010 TESTS", "true")
            wallet.transfer(creator, receiver, "1.000 TESTS", "", "true")

            transfer_result = wallet.transfer(receiver, receiver, "0.001 TESTS", "", "true")
            assert "error" in transfer_result, "Should not be able to transfer with no RC"
            assert "Please wait to transact" in transfer_result['error']['message'], "Should not be able to transfer with no RC"

            rc_receiver = wallet.find_rc_accounts([receiver])['result'][0]
            rc_delegator = wallet.find_rc_accounts([delegator])['result'][0]
            rc_delegator_before = rc_delegator

            assert(rc_receiver['delegated_rc'] == 0)
            assert(rc_receiver['received_delegated_rc'] == 0)
            assert(rc_delegator['delegated_rc'] == 0)
            assert(rc_delegator['received_delegated_rc'] == 0)

            log.info("Delegating rc to {}".format(receiver))
            wallet.delegate_rc(delegator, receiver, 10, "true")

            rc_receiver = wallet.find_rc_accounts([receiver])['result'][0]
            rc_delegator = wallet.find_rc_accounts([delegator])['result'][0]

            assert(rc_receiver['delegated_rc'] == 0)
            assert(rc_receiver['received_delegated_rc'] == 10)
            assert(rc_delegator['delegated_rc'] == 10)
            assert(rc_delegator['received_delegated_rc'] == 0)
            assert(rc_delegator['rc_manabar']['current_mana'] == rc_delegator_before['rc_manabar']['current_mana'] - 13)  # 10 + 3, 3 is the base cost of an rc delegation op, 10 is the amount delegated

            log.info("testing list direct delegations api")

            delegation_from_to = wallet.list_rc_direct_delegations([delegator, receiver], 1000, "by_from_to")['result'][0]

            assert(delegation_from_to['from'] == delegator)
            assert(delegation_from_to['to'] == receiver)
            assert(delegation_from_to['delegated_rc'] == 10)

            log.info("Increasing the delegation to 50 to {}".format(receiver))
            wallet.delegate_rc(delegator, receiver, 50, "true")

            rc_receiver = wallet.find_rc_accounts([receiver])['result'][0]
            rc_delegator = wallet.find_rc_accounts([delegator])['result'][0]

            assert(rc_receiver['delegated_rc'] == 0)
            assert(rc_receiver['received_delegated_rc'] == 50)
            assert(rc_delegator['delegated_rc'] == 50)
            assert(rc_delegator['received_delegated_rc'] == 0)
            assert(rc_delegator['rc_manabar']['current_mana'] == rc_delegator_before['rc_manabar']['current_mana'] - 53)  # 50 + 3, 3 is the base cost of an rc delegation op, 15 is the amount delegated

            log.info("Reducing the delegation to 20 to {}".format(receiver))
            wallet.delegate_rc(delegator, receiver, 20, "true")

            rc_receiver = wallet.find_rc_accounts([receiver])['result'][0]
            rc_delegator = wallet.find_rc_accounts([delegator])['result'][0]

            assert(rc_receiver['delegated_rc'] == 0)
            assert(rc_receiver['received_delegated_rc'] == 20)
            assert(rc_delegator['delegated_rc'] == 20)
            assert(rc_delegator['received_delegated_rc'] == 0)
            assert(rc_delegator['rc_manabar']['current_mana'] == rc_delegator_before['rc_manabar']['current_mana'] - 53)  # amount remains the same because current rc is not given back from reducing the delegation

            log.info("deleting the delegation to {}".format(receiver))
            wallet.delegate_rc(delegator, receiver, 0, "true")

            rc_receiver = wallet.find_rc_accounts([receiver])['result'][0]
            rc_delegator = wallet.find_rc_accounts([delegator])['result'][0]
            delegation = wallet.list_rc_direct_delegations([delegator, receiver], 1000, "by_from_to")['result']

            assert(rc_receiver['delegated_rc'] == 0)
            assert(len(delegation) == 0)
            assert(rc_receiver['received_delegated_rc'] == 0)
            assert(rc_delegator['delegated_rc'] == 0)
            assert(rc_delegator['received_delegated_rc'] == 0)
            assert(rc_delegator['max_rc'] == rc_delegator_before['max_rc'])
