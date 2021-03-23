#!/usr/bin/python3

import time

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet( args ) as wallet:
            creator, receiver = make_user_for_tests(wallet)
            wallet.post_comment(creator, "lorem", "", "ipsum", "Lorem Ipsum", "body", "{}", "true")
            wallet.create_proposal(creator, receiver, "2029-06-02T00:00:00", "2029-08-01T00:00:00", "10.000 TBD", "this is subject", "lorem", "true")
            proposals = find_creator_proposals(creator, wallet.list_proposals([creator], 50, "by_creator", "ascending", "all"))

            proposal_id = -1
            for proposal in proposals:
                if proposal['receiver'] == receiver:
                    proposal_id = proposal['proposal_id']
                    break

            assert proposal_id != -1, "Proposal just created was not found"

            log.info("testing updating the proposal with the end date")
            output = wallet.update_proposal(proposal_id, creator, "9.000 TBD", "this is an updated subject", "lorem", "2029-07-25T00:00:00", "true")
            proposal = wallet.find_proposals([proposal_id])['result'][0]

            assert(proposal['daily_pay'] == "9.000 TBD")
            assert(proposal['subject'] == "this is an updated subject")
            assert(proposal['permlink'] == "lorem")
            assert(proposal['end_date'] == "2029-07-25T00:00:00")

            log.info("testing updating the proposal without the end date")
            test = wallet.update_proposal(proposal_id, creator, "8.000 TBD", "this is an updated subject again", "lorem", None, "true")
            proposal = wallet.find_proposals([proposal_id])['result'][0]

            assert(proposal['daily_pay'] == "8.000 TBD")
            assert(proposal['subject'] == "this is an updated subject again")
            assert(proposal['permlink'] == "lorem")
            assert(proposal['end_date'] == "2029-07-25T00:00:00")
