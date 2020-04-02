#!/usr/bin/python3

import time
import json

#from ... import utils
from utils.test_utils import *
from utils.cmd_args   import args
from utils.cli_wallet import CliWallet
from utils.logger     import log, init_logger

def get_votes( wallet, account_name ):

    pattern = "get_account" + " \"" + account_name + "\""

    response = wallet.get_account( account_name )

    found = response.find( pattern )
    if found == -1:
        return 0

    found += len( pattern )
    sub_response = response[ found : len( response ) ]

    items = json.loads( sub_response )

    assert( "delayed_votes" in items )

    dv_items = items["delayed_votes"]

    val = 0
    for item in dv_items:
        assert( "val" in item )
        val += int( item[ "val" ] )

    return val

if __name__ == "__main__":
    try:
        init_logger(__file__)

        error = False
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
        log.info( "actors:  creator: {0} receiver: ".format( creator, receiver ) )

        #   "delayed_votes": [{
        #       "time": "2020-04-02T12:39:33",
        #       "val": 642832
        #     }
        #   ]

        #=================Adding delayed votes=================
        before_val = get_votes( wallet, receiver )

        #=================Vest liquid again=================
        wallet.transfer_to_vesting( creator, receiver, "1.000 TESTS", "true" )

        #=================Adding delayed votes=================
        after_val = get_votes( wallet, receiver )

        log.info( "before_val: {0} after_val: {1}".format( before_val, after_val ) )

        #=================Checking changes=================
        assert( after_val > before_val )

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

