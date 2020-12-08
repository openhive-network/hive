#!/usr/bin/python3

from collections      import OrderedDict
import json

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet( args ) as wallet:
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
