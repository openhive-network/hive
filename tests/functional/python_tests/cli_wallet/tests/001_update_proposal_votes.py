#!/usr/bin/python3

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet( args ) as wallet:
            creator, receiver = make_user_for_tests(wallet)

            proposals_before = len(find_voter_proposals(creator, last_message_as_json( wallet.list_proposal_votes([creator], 20, "by_voter_proposal", "ascending", "all"))))
            log.info("proposals_before {0}".format(proposals_before))

            wallet.post_comment(creator, "lorem", "", "ipsum", "Lorem Ipsum", "body", "{}", "true")
            wallet.create_proposal(creator, receiver, "2029-06-02T00:00:00", "2029-08-01T00:00:00", "1.000 TBD", "this is subject", "lorem", "true")
            
            resp = find_creator_proposals(creator, last_message_as_json(wallet.list_proposals([creator], 50, "by_creator", "ascending", "all")))
            new_proposal_id = 0
            for r in resp:
                if r["id"] > new_proposal_id:
                    new_proposal_id = r["id"]

            proposals_middle = len(find_voter_proposals(creator, last_message_as_json( wallet.list_proposal_votes([creator], 20, "by_voter_proposal", "ascending", "all"))))
            log.info("proposals_middle {0}".format(proposals_middle))

            wallet.update_proposal_votes(creator, [new_proposal_id], "true", "true")
            proposals_after = len(find_voter_proposals(creator, last_message_as_json( wallet.list_proposal_votes([creator], 20, "by_voter_proposal", "ascending", "all"))))

            log.info("proposals_after {0}".format(proposals_after))

            assert proposals_before == proposals_middle, "proposals_before should be equal to proposals_middle."
            assert proposals_middle + 1 == proposals_after, "proposals_middle +1 should be equal to proposals_after."
