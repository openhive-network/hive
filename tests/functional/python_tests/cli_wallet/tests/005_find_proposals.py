#!/usr/bin/python3

from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

if __name__ == "__main__":
    with Test(__file__):
        with CliWallet( args ) as wallet:
            id_sets = [ [1], [2], [3], [1,2], [1,2,3], [2,3], [3,4], [4,5], [1,2,3,4,5,6,7]]

            for ids in id_sets:
                call_args = {"id_set":ids}
                resp = last_message_as_json(wallet.find_proposals(ids))
                if resp: 
                    if "error" in resp:
                        raise ArgsCheckException("Some error occures.")
                else:
                    raise ArgsCheckException("Parse error.")
