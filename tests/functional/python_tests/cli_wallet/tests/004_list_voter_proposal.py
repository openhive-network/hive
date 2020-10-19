#!/usr/bin/python3

from collections      import OrderedDict
import json

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

if __name__ == "__main__":
    with Test(__file__):
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

        active          = [ "all",
                            "inactive",
                            "active",
                            "expired",
                            "votable"]
        order_by        = [ "by_voter_proposal"
                            ]
        order_type = ["ascending",
                      "descending"]

        for by in order_by:
            for direct in  order_type:
                for act in active:
                    resp=last_message_as_json(wallet.list_proposal_votes([args.creator], 10, by, direct, act))
                    if resp: 
                        if "error" in resp:
                            raise ArgsCheckException("Some error occures.")
                    else:
                        raise ArgsCheckException("Parse error.")

    except Exception as _ex:
        log.exception(str(_ex))
        error = True
    finally:
        if error:
            log.error("TEST `{0}` failed".format(__file__))
            exit(1)
        else:
            log.info("TEST `{0}` passed".format(__file__))
            exit(0)

