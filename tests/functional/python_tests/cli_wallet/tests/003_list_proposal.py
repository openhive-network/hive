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
            order_by        = [ "by_creator",
                                "by_start_date",
                                "by_end_date",
                                "by_total_votes"
                                ]
            order_type = ["ascending",
                        "descending"]

            for by in order_by:
                for direct in  order_type:
                    for act in active:
                        if by == "by_start_date" or by == "by_end_date":
                            resp=last_message_as_json(wallet.list_proposals(["2019-03-01T00:00:00"], 10, by, direct, act))
                        elif by == "by_total_votes":
                            resp=last_message_as_json(wallet.list_proposals([0], 10, by, direct, act))
                        else:
                            resp=last_message_as_json(wallet.list_proposals([args.creator], 10, by, direct, act))
                        if resp: 
                            if "error" in resp:
                                raise ArgsCheckException("Some error occures.")
                        else:
                            raise ArgsCheckException("Parse error.")
