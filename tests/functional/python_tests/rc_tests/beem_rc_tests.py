#!/usr/bin/python3

import sys
sys.path.append("../../")
import hive_utils

import logging
import test_utils
import os

return_code = 0

LOG_LEVEL = logging.INFO
LOG_FORMAT = "%(asctime)-15s - %(name)s - %(levelname)s - %(message)s"
MAIN_LOG_PATH = "rc_tests_001.log"
log_dir = os.environ.get("TEST_LOG_DIR", None)
if log_dir is not None:
    MAIN_LOG_PATH = log_dir + "/" + MAIN_LOG_PATH
else:
    MAIN_LOG_PATH = "./" + MAIN_LOG_PATH


MODULE_NAME = "RC-Tests"
logger = logging.getLogger(MODULE_NAME)
logger.setLevel(LOG_LEVEL)

ch = logging.StreamHandler(sys.stdout)
ch.setLevel(LOG_LEVEL)
ch.setFormatter(logging.Formatter(LOG_FORMAT))

fh = logging.FileHandler(MAIN_LOG_PATH)
fh.setLevel(LOG_LEVEL)
fh.setFormatter(logging.Formatter(LOG_FORMAT))

if not logger.hasHandlers():
    logger.addHandler(ch)
    logger.addHandler(fh)

try:
    from beem import Hive
except Exception as ex:
    logger.error("beem library is not installed.")
    sys.exit(1)

# Greedy baby scenario
# 0. In this scenario we have one proposal with huge daily pay and couple with low daily pay
#    all proposals have the same number of votes, greedy proposal is last
# 1. create few proposals - in this scenario proposals have different starting and ending dates
# 2. vote on them to show differences in asset distribution (depending on collected votes)
# 3. wait for proposal payment phase
# 4. verify (using account history and by checking regular account balance) that given accounts have been correctly paid.

# Expected result: all got paid.


if __name__ == '__main__':
    logger.info("Performing RC tests")
    from beem.rc import RC
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("creator", help = "Account to create test accounts with")
    parser.add_argument("wif", help="Private key for creator account")
    parser.add_argument("--node-url", dest="node_url", default="http://127.0.0.1:8090", help="Url of working hive node")
    parser.add_argument("--run-hived", dest="hived_path", help = "Path to hived executable. Warning: using this option will erase contents of selected hived working directory.")
    parser.add_argument("--working-dir", dest="hived_working_dir", default="/tmp/hived-data/", help = "Path to hived working directory")
    parser.add_argument("--config-path", dest="hived_config_path", default="../../hive_utils/resources/config.ini.in",help = "Path to source config.ini file")

    args = parser.parse_args()
    node = None

    if args.hived_path:
        logger.info("Running hived via {} in {} with config {}".format(args.hived_path,
                                                                       args.hived_working_dir,
                                                                       args.hived_config_path)
                    )

        node = hive_utils.hive_node.HiveNodeInScreen(
            args.hived_path,
            args.hived_working_dir,
            args.hived_config_path
        )

    node_url = args.node_url
    wif = args.wif

    if len(wif) == 0:
        logger.error("Private-key is not set in config.ini")
        sys.exit(1)

    logger.info("Using node at: {}".format(node_url))
    logger.info("Using private-key: {}".format(wif))

    accounts = [
        # place accounts here in the format: {'name' : name, 'private_key' : private-key, 'public_key' : public-key}
        {"name" : "tester001", "private_key" : "5KQeu7SdzxT1DiUzv7jaqwkwv1V8Fi7N8NBZtHugWYXqVFH1AFa", "public_key" : "TST8VfiahQsfS1TLcnBfp4NNfdw67uWweYbbUXymbNiDXVDrzUs7J"},
        {"name" : "pool", "private_key" : "5KgfcV9bgEen3v9mxkoGw6Rhuf2giDRZTHZjzwisjkrpF4FUh3N", "public_key" : "TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn"},
    ]
    account_names = [ v['name'] for v in accounts ]

    if not accounts:
        logger.error("Accounts array is empty, please add accounts in a form {\"name\" : name, \"private_key\" : private_key, \"public_key\" : public_key}")
        sys.exit(1)

    keys = [wif]
    for account in accounts:
        keys.append(account["private_key"])

    if node is not None:
        node.run_hive_node(["--enable-stale-production"])
    try:
        if node is None or node.is_running():
            node_client = Hive(node=[node_url], no_broadcast=False,
                               keys=keys
                               )
            rc = RC(node_client)

            test_utils.create_accounts(node_client, args.creator, accounts)
            test_utils.transfer_to_vesting(node_client, args.creator, 'pool', "300.000","TESTS")

            logger.info("Testing delegating rc to pool")
            pool = node_client.rpc.find_rc_delegation_pools(["pool"])
            assert len(pool) == 0
            rc.delegate_to_pool("initminer", 'pool', '100')
            pool = node_client.rpc.find_rc_delegation_pools(["pool"])[0]
            assert pool['account'] == 'pool'
            assert pool['max_rc'] == 100
            assert pool['rc_pool_manabar']['current_mana'] == 100

            rc.delegate_to_pool("pool", "pool", "150")

            pool = node_client.rpc.find_rc_delegation_pools(["pool"])[0]
            assert pool['account'] == 'pool'
            assert pool['max_rc'] == 250
            assert pool['rc_pool_manabar']['current_mana'] == 150 # Expected

            logger.info("Testing setting a delegator slot")

            result = node_client.rpc.find_rc_accounts(["tester001"])[0]
            assert len(result['delegation_slots']) == 3
            assert result['delegation_slots'][0]['delegator'] == args.creator
            assert(result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][0]['max_mana'] == 0)

            assert(result['delegation_slots'][1]['delegator'] == "")
            assert(result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][1]['max_mana'] == 0)

            assert(result['delegation_slots'][2]['delegator'] == "")
            assert(result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][2]['max_mana'] == 0)
            
            rc.set_slot_delegator("pool", "tester001", 0, args.creator)
            result = node_client.rpc.find_rc_accounts(["tester001"])[0]
            assert len(result['delegation_slots']) == 3
            assert(result['delegation_slots'][0]['delegator'] == "pool")
            assert(result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][0]['max_mana'] == 0)

            assert(result['delegation_slots'][1]['delegator'] == "")
            assert(result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][1]['max_mana'] == 0)

            assert(result['delegation_slots'][2]['delegator'] == "")
            assert(result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][2]['max_mana'] == 0)

            logger.info("Testing delegating rc")

            rc.delegate_from_pool("pool", "tester001", 50)
            result = node_client.rpc.find_rc_accounts(["tester001"])[0]
            assert(result['max_rc'] == 0)
            assert len(result['delegation_slots']) == 3
            assert(result['delegation_slots'][0]['delegator'] == "pool")
            assert(result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 50)
            assert(result['delegation_slots'][0]['max_mana'] == 50)
            assert(result['delegation_slots'][1]['delegator'] == "")
            assert(result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][1]['max_mana'] == 0)
            assert(result['delegation_slots'][2]['delegator'] == "")
            assert(result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][2]['max_mana'] == 0)

            logger.info("Testing trying to change the slot with delegated rc (thus removing the delegation")
            try:
                rc.set_slot_delegator(args.creator, "tester001", 0, "tester001")
            except Exception:
                pass
            result = node_client.rpc.find_rc_accounts(["tester001"])[0]
            assert len(result['delegation_slots']) == 3
            assert(result['delegation_slots'][0]['delegator'] == "pool")
            assert(result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 50)
            assert(result['delegation_slots'][0]['max_mana'] == 50)
            assert(result['delegation_slots'][1]['delegator'] == "")
            assert(result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][1]['max_mana'] == 0)
            assert(result['delegation_slots'][2]['delegator'] == "")
            assert(result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
            assert(result['delegation_slots'][2]['max_mana'] == 0)
        else:
            raise Exception("no node detected")
    except Exception as ex:
        logger.error("Exception : {}".format(ex))
        return_code = 1
    finally:
        if node is not None:
            # input()
            node.stop_hive_node()
        sys.exit( return_code )

