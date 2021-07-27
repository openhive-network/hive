#!/usr/bin/python3

import time

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger
import datetime

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet( args ) as wallet:
            creator, receiver = make_user_for_tests(wallet)

            recurrent_transfers_before_count = len(wallet.find_recurrent_transfers(creator)['result'])
            log.info("recurrent_transfers {0}".format(recurrent_transfers_before_count))

            create_prop = wallet.recurrent_transfer(creator, receiver, "10.000 TBD", "This is a memo", "24", "6", "true")

            recurrent_transfers = wallet.find_recurrent_transfers(creator)['result']
            recurrent_transfers_after_count = len(recurrent_transfers)
            log.info("recurrent_transfers_after_count {0}".format(recurrent_transfers_after_count))
            recurrent_transfer = recurrent_transfers[recurrent_transfers_after_count - 1]

            assert recurrent_transfers_before_count + 1 == recurrent_transfers_after_count, "recurrent_transfers_before_count +1 should be equal to recurrent_transfers_after_count."
            assert recurrent_transfer['from'] == creator
            assert recurrent_transfer['to'] == receiver
            assert recurrent_transfer['amount'] == '10.000 TBD'
            assert recurrent_transfer['memo'] == 'This is a memo'
            assert recurrent_transfer['recurrence'] == 24
            assert recurrent_transfer['consecutive_failures'] == 0
            assert recurrent_transfer['remaining_executions'] == 5
