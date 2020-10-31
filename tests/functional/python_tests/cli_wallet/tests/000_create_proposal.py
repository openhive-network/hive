#!/usr/bin/python3

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

if __name__ == "__main__":
<<<<<<< HEAD
    with Test(__file__):
=======
    try:
        init_logger(__file__)
        log.info("Starting test: {0}".format(__file__))
        error = False
>>>>>>> [WK] Enable cli wallet tests in cmake.
        wallet = CliWallet( args.path,
                            args.server_rpc_endpoint,
                            args.cert_auth,
                            args.rpc_tls_endpoint,
                            args.rpc_tls_cert,
                            args.rpc_http_endpoint,
                            args.deamon, 
                            args.rpc_allowip,
                            args.wallet_file,
                            args.chain_id,
                            args.wif )
        wallet.set_and_run_wallet()

        creator, receiver = make_user_for_tests(wallet)

        proposals_before = len(find_creator_proposals(creator, last_message_as_json( wallet.list_proposals([creator], 50, "by_creator", "ascending", "all"))))
        log.info("proposals_before {0}".format(proposals_before))

        wallet.post_comment(creator, "lorem", "", "ipsum", "Lorem Ipsum", "body", "{}", "true")
        create_prop = wallet.create_proposal(creator, receiver, "2029-06-02T00:00:00", "2029-08-01T00:00:00", "1.000 TBD", "this is subject", "lorem", "true")

        proposals_after = len(find_creator_proposals(creator, last_message_as_json( wallet.list_proposals([creator], 50, "by_creator", "ascending", "all"))))
        log.info("proposals_after {0}".format(proposals_after))

        assert proposals_before +1 == proposals_after, "proposals_before +1 should be equal to proposals_after."
