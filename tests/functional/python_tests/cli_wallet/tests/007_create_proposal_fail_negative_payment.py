#!/usr/bin/python3

import time

#from ... import utils
from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet( args ) as wallet:
            creator, receiver = make_user_for_tests(wallet)

            proposals_before = len(find_creator_proposals(creator, last_message_as_json( wallet.list_proposals([creator], 50, "by_creator", "ascending", "all"))))
            log.info("proposals_before {0}".format(proposals_before))

            wallet.post_comment(creator, "lorem", "", "ipsum", "Lorem Ipsum", "body", "{}", "true")
            create_prop = json.dumps(wallet.create_proposal(creator, receiver, "2029-06-02T00:00:00", "2029-08-01T00:00:00", "-1.000 TBD", "this is subject", "lorem", "true"))
            if not create_prop.find("daily_pay.amount >= 0: Daily pay can't be negative value") != -1:
                raise ArgsCheckException("Assertion `{0}` is required.".format("daily_pay.amount >= 0: Daily pay can't be negative value"))

            proposals_after = len(find_creator_proposals(creator, last_message_as_json( wallet.list_proposals([creator], 50, "by_creator", "ascending", "all"))))
            log.info("proposals_after {0}".format(proposals_after))
            
            if not proposals_before == proposals_after:
                raise ArgsCheckException("proposals_before +1 should be equal to proposals_after.")
