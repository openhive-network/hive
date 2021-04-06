#!/usr/bin/python3

import sys
sys.path.append("../../")
import hive_utils

import logging
import test_utils
import os
from junit_xml import TestSuite

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


def test_delegate_to_pool():
    rc = RC(node_client)
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



def test_set_delegator_slot():
    rc = RC(node_client)
    logger.info("Testing setting a delegator slot")

    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
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

    rc.set_slot_delegator("pool", "rctest1", 0, args.creator)
    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
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

def test_delegate_rc():
    rc = RC(node_client)
    logger.info("Testing delegating rc")
    rc.delegate_from_pool("pool", "rctest1", 50)
    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
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

def test_set_slot_remove_rc():
    rc = RC(node_client)
    logger.info("Testing trying to change the slot with delegated rc, thus removing the delegation, leaving the user with no RC")
    try:
        rc.set_slot_delegator(args.creator, "rctest1", 0, "rctest1")
    except Exception:
        pass
    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
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


def test_set_slot():
    rc = RC(node_client)
    logger.info("set_slot_delegator full tests")
    try:
        rc.set_slot_delegator(args.creator, "rctest1", 0, "alice")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to set creator slot when you aren't the creator or user"

    try:
        rc.set_slot_delegator(args.creator, "rctest1", 1, "alice")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to set recovery slot when you aren't the recovery partner or user"

    try:
        rc.set_slot_delegator(args.creator, "rctest1", 2, "alice")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to set user slot when you aren't the user"

    try:
        rc.set_slot_delegator(args.creator, "rctest1", 3, "alice")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to set a user controlled slot that doesn't exist"

    try:
        rc.set_slot_delegator(args.creator, "rctest1", 3, "rctest1")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to set a slot that doesn't exist"

    try:
        rc.set_slot_delegator("randomaccount", "rctest1", 2, "rctest1")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to set a slot to an account that doesn't exist"

    try:
        rc.set_slot_delegator("pool", "rctest1", 2, "rctest1")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to set a slot to the same account twice"

    logger.info("change slot on a slot receiving rc, deleting the delegation")
    rc.set_slot_delegator(args.creator, "rctest1", 0, args.creator)
    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
    assert len(result['delegation_slots']) == 3
    assert(result['delegation_slots'][0]['delegator'] == args.creator)
    assert(result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
    assert(result['delegation_slots'][0]['max_mana'] == 0)
    assert(result['delegation_slots'][1]['delegator'] == "")
    assert(result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert(result['delegation_slots'][1]['max_mana'] == 0)
    assert(result['delegation_slots'][2]['delegator'] == "")
    assert(result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert(result['delegation_slots'][2]['max_mana'] == 0)


def test_delegate_to_pool_full():
    rc = RC(node_client)
    logger.info("delegate rc to pool full tests")
    try:
        rc.delegate_to_pool("alice", 'randomacco', '0')
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to delegate to nonexisting pool"
    try:
        rc.delegate_to_pool("alice", 'alice', '0')
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to delegate 0 to a non initialized pool"

    # refresh RC
    rc.delegate_to_pool("alice", 'alice', '1')
    rc.delegate_to_pool("alice", 'alice', '0')
    rc_account_before = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account_before['out_delegation_total'] == 0, "out_delegation_total is not 0"
    rc.delegate_to_pool("alice", 'alice', '100')
    rc_account_after = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account_before['rc_manabar']['current_mana'] - rc_account_after['rc_manabar']['current_mana'] > 100, "Current rc was not affected by the delegation to the pool, delta is {}".format(rc_account_before['rc_manabar']['current_mana'] - rc_account_after['rc_manabar']['current_mana'])
    assert rc_account_before['max_rc'] - rc_account_after['max_rc'] == 100, "max rc was not affected by the delegation to the pool"
    assert rc_account_after['out_delegation_total'] == 1, "out_delegation_total was not changed"
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 100
    assert pool['rc_pool_manabar']['current_mana'] == 100

    rc_account = node_client.rpc.find_rc_accounts(["alice"])[0]
    try:
        rc.delegate_to_pool("alice", 'alice', str(rc_account['rc_manabar']['max_rc']))
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to delegate all the RC"

    try:
        rc.delegate_to_pool("alice", 'alice', '999999999999999999')
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to delegate more rc than the account has"

    logger.info("Testing emptying an rc pool with a delegation to an account")
    rc.set_slot_delegator("alice", "rctest2", 0, args.creator)
    rc.delegate_from_pool("alice", "rctest2", 50)
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 100
    assert pool['rc_pool_manabar']['current_mana'] == 100
    result = node_client.rpc.find_rc_accounts(["rctest2"])[0]
    before_current_mana = result['delegation_slots'][0]["rc_manabar"]["current_mana"]
    assert len(result['delegation_slots']) == 3
    assert(result['delegation_slots'][0]['delegator'] == "alice")
    assert(result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 50)
    assert(result['delegation_slots'][0]['max_mana'] == 50)
    assert(result['delegation_slots'][1]['delegator'] == "")
    assert(result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert(result['delegation_slots'][1]['max_mana'] == 0)
    assert(result['delegation_slots'][2]['delegator'] == "")
    assert(result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert(result['delegation_slots'][2]['max_mana'] == 0)

    rc_account_before = node_client.rpc.find_rc_accounts(["alice"])[0]

    # tx used to use some RC
    rc.set_slot_delegator("null", "rctest2", 2, "rctest2")
    hive_utils.common.wait_n_blocks(args.node_url, 2)
    # refresh rc
    rc.delegate_to_pool("alice", 'alice', '0')
    rc_account_after = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account_after['rc_manabar']['current_mana'] - rc_account_before['rc_manabar']['current_mana'] > 75, "Current rc was not affected by the removal of the delegation to the pool"
    assert rc_account_after['max_rc'] - rc_account_before['max_rc'] == 100, "max rc was not affected by the removal of the delegation to the pool"

    result = node_client.rpc.find_rc_accounts(["rctest2"])[0]
    assert len(result['delegation_slots']) == 3
    assert(result['delegation_slots'][0]['delegator'] == "alice")
    assert(result['delegation_slots'][0]["rc_manabar"]["current_mana"] < before_current_mana)
    assert(result['delegation_slots'][0]['max_mana'] == 50)
    assert(result['delegation_slots'][1]['delegator'] == "")
    assert(result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert(result['delegation_slots'][1]['max_mana'] == 0)
    assert(result['delegation_slots'][2]['delegator'] == "null")
    assert(result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert(result['delegation_slots'][2]['max_mana'] == 0)

    try:
        rc.set_slot_delegator("rctest3", "rctest2", 1, "rctest2")
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to use rc with an empty pool"

    logger.info("Test changing the max_rc of a pool")
    rc.delegate_to_pool("alice", 'alice', '800')
    hive_utils.common.wait_n_blocks(args.node_url, 2)
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 800
    assert pool['rc_pool_manabar']['current_mana'] == 800

    rc.delegate_to_pool("alice", 'alice', '400')
    hive_utils.common.wait_n_blocks(args.node_url, 2)
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 400
    assert pool['rc_pool_manabar']['current_mana'] == 400

    rc.delegate_to_pool(args.creator, 'alice', '100')
    hive_utils.common.wait_n_blocks(args.node_url, 2)
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 500
    assert pool['rc_pool_manabar']['current_mana'] == 500

    rc.delegate_to_pool("alice", 'alice', '0')
    hive_utils.common.wait_n_blocks(args.node_url, 2)
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 100
    assert pool['rc_pool_manabar']['current_mana'] == 100

    logger.info("Test changing the max_rc of a pool when you don't have enough current_rc to match your max_rc delegation")
    rc_account = node_client.rpc.find_rc_accounts(["alice"])[0]
    rc.delegate_to_pool("alice", 'initminer', str(rc_account['max_rc'] - 200))
    rc_account1 = node_client.rpc.find_rc_accounts(["alice"])[0]
    # use RC
    test_utils.create_post(node_client, "alice", "permlink123")
    # delegate 190 when the account's current_mana is less than that
    pool1 = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    rc.delegate_to_pool("alice", 'alice', "190")
    rc_account2 = node_client.rpc.find_rc_accounts(["alice"])[0]
    pool2 = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 290
    assert pool['rc_pool_manabar']['current_mana'] < pool['max_rc'], "current pool mana should be less than the max rc from the pool"



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
    parser.add_argument("--junit-output", dest="junit_output", default=None, help="Filename for generating jUnit-compatible XML output")

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
        {"name" : "pool", "private_key" : "5KgfcV9bgEen3v9mxkoGw6Rhuf2giDRZTHZjzwisjkrpF4FUh3N", "public_key" : "TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn"},
        {"name" : "alice", "private_key" : "5KgxYhw6yK9QrLKQGAxRqauGuGycMVF1QNbE43Sr3E28Lo54cYq", "public_key" : "TST73QDXtjvmc97qGDeCmA8Why7JHnWjzkpEuW6Htr56R2DJ6d58q"},
        {"name" : "rctest1", "private_key" : "5KQeu7SdzxT1DiUzv7jaqwkwv1V8Fi7N8NBZtHugWYXqVFH1AFa", "public_key" : "TST8VfiahQsfS1TLcnBfp4NNfdw67uWweYbbUXymbNiDXVDrzUs7J"},
        {"name" : "rctest2", "private_key" : "5JcQCDVnu7v7PWpsKUD8TcQqhB1v2WvSbfhK8JzDywdKEWBddNX", "public_key" : "TST6rb6ZbcvqufSHbpt6gAWT4YniDHLnMvU7QoEZbRvNG31USzYnX"},
        {"name" : "rctest3", "private_key" : "5J1QbAFcF9Xp9gVtvNMXbmzA16Cy15nKjCCZiVQa2ZxMmhZQnBd", "public_key" : "TST7vFHFVKpH9R37e6M6n7CkV6CAQZFR41Gc2f4nxNu7EW9347shb"},
        {"name" : "rctest4", "private_key" : "5JfLZHtGWBP8NjNkP5xkxf6GvvMaY4ZGanSzm7qSmDCrQFq22uy", "public_key" : "TST6NrizgPtefvuQV1g43Q2mKJKy1YAQMyhQJhRm4g4PtEtGvhn4N"},
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

    node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)
    test_utils.create_accounts(node_client, args.creator, accounts)
    test_utils.transfer_to_vesting(node_client, args.creator, 'pool', "300.000", "TESTS")
    test_utils.transfer_to_vesting(node_client, args.creator, 'alice', "300.000", "TESTS")
    #test_delegate_to_pool()
    #test_set_delegator_slot()
    #test_delegate_rc()
    #test_set_slot_remove_rc()
    #test_set_slot()
    #test_delegate_to_pool_full()
    logger.info("Test changing the max_rc of a pool")
    rc = RC(node_client)
    rc.delegate_to_pool("alice", 'alice', '800')
    hive_utils.common.wait_n_blocks(args.node_url, 2)
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 800
    assert pool['rc_pool_manabar']['current_mana'] == 800

    rc.delegate_to_pool("alice", 'alice', '400')
    hive_utils.common.wait_n_blocks(args.node_url, 2)
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 400
    assert pool['rc_pool_manabar']['current_mana'] == 400

    rc.delegate_to_pool(args.creator, 'alice', '100')
    hive_utils.common.wait_n_blocks(args.node_url, 2)
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 500
    assert pool['rc_pool_manabar']['current_mana'] == 500

    '''
    try:
        if node is None or node.is_running():
            node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)
            test_utils.create_accounts(node_client, args.creator, accounts)
            test_utils.transfer_to_vesting(node_client, args.creator, 'pool', "300.000","TESTS")
            test_utils.transfer_to_vesting(node_client, args.creator, 'alice', "300.000","TESTS")
            test_delegate_to_pool()
            test_set_delegator_slot()
            test_delegate_rc()
            test_set_slot_remove_rc()
            test_set_slot()
            test_delegate_to_pool_full()

        else:
            raise Exception("no node detected")
    except Exception as ex:
        logger.error("Exception : {}".format(ex))
        return_code = 1
    finally:
        if node is not None:
            node.stop_hive_node()
        if args.junit_output is not None:
            test_suite = TestSuite('dhf_tests', hive_utils.common.junit_test_cases)
            with open(args.junit_output, "w") as junit_xml:
                TestSuite.to_file(junit_xml, [test_suite], prettyprint=False)
        sys.exit( return_code )
'''
