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

from beem.account import Account


def test_delegate_to_pool():
    rc = RC(node_client)
    logger.info("Testing delegating rc to pool")
    pool = node_client.rpc.find_rc_delegation_pools(["pool"])
    assert len(pool) == 0
    rc.delegate_to_pool("initminer", 'pool', '100')
    pool = node_client.rpc.find_rc_delegation_pools(["pool"])[0]
    rc_delegation_to_pool = node_client.rpc.find_rc_delegations("initminer")[0]
    assert pool['account'] == 'pool'
    assert pool['max_rc'] == 100
    assert pool['rc_pool_manabar']['current_mana'] == 100
    assert rc_delegation_to_pool['from_account'] == args.creator
    assert rc_delegation_to_pool['to_pool'] == 'pool'
    assert rc_delegation_to_pool['amount']['amount'] == '100'

    rc.delegate_to_pool("pool", "pool", "150")

    pool = node_client.rpc.find_rc_delegation_pools(["pool"])[0]
    assert pool['account'] == 'pool'
    assert pool['max_rc'] == 250
    assert pool['rc_pool_manabar']['current_mana'] == 250


def test_set_delegator_slot():
    rc = RC(node_client)
    logger.info("Testing setting a delegator slot")

    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
    assert len(result['delegation_slots']) == 3
    assert result['delegation_slots'][0]['delegator'] == args.creator
    assert (result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][0]['max_mana'] == 0)

    assert (result['delegation_slots'][1]['delegator'] == "")
    assert (result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][1]['max_mana'] == 0)

    assert (result['delegation_slots'][2]['delegator'] == "")
    assert (result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][2]['max_mana'] == 0)

    rc.set_slot_delegator("pool", "rctest1", 0, args.creator)
    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
    assert len(result['delegation_slots']) == 3
    assert (result['delegation_slots'][0]['delegator'] == "pool")
    assert (result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][0]['max_mana'] == 0)

    assert (result['delegation_slots'][1]['delegator'] == "")
    assert (result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][1]['max_mana'] == 0)

    assert (result['delegation_slots'][2]['delegator'] == "")
    assert (result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][2]['max_mana'] == 0)


def test_delegate_rc():
    rc = RC(node_client)
    logger.info("Testing delegating rc")
    rc.delegate_from_pool("pool", "rctest1", 50)
    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
    assert (result['max_rc'] == 0)
    assert len(result['delegation_slots']) == 3
    assert (result['delegation_slots'][0]['delegator'] == "pool")
    assert (result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 50)
    assert (result['delegation_slots'][0]['max_mana'] == 50)
    assert (result['delegation_slots'][1]['delegator'] == "")
    assert (result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][1]['max_mana'] == 0)
    assert (result['delegation_slots'][2]['delegator'] == "")
    assert (result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][2]['max_mana'] == 0)


def test_set_slot_remove_rc():
    rc = RC(node_client)
    logger.info("Testing trying to change the slot with delegated rc, thus removing the delegation, leaving the user with no RC")
    try:
        rc.set_slot_delegator(args.creator, "rctest1", 0, "rctest1")
    except Exception:
        pass
    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
    assert len(result['delegation_slots']) == 3
    assert (result['delegation_slots'][0]['delegator'] == "pool")
    assert (result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 50)
    assert (result['delegation_slots'][0]['max_mana'] == 50)
    assert (result['delegation_slots'][1]['delegator'] == "")
    assert (result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][1]['max_mana'] == 0)
    assert (result['delegation_slots'][2]['delegator'] == "")
    assert (result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][2]['max_mana'] == 0)


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
        rc.set_slot_delegator("rctest1", "rctest1", 2, "randomacct")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to sign with an account that doesn't exists"

    try:
        rc.set_slot_delegator("pool", "rctest1", 2, "rctest1")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to set a slot to the same account twice"

    try:
        rc.set_slot_delegator("pool", "rctest1", 0, "rctest1")
    except Exception:
        pass
    else:
        assert False, "shouldn't be able to set the same slot delegator on the same slot"

    logger.info("change slot on a slot receiving rc, deleting the delegation")
    rc.set_slot_delegator(args.creator, "rctest1", 0, args.creator)
    result = node_client.rpc.find_rc_accounts(["rctest1"])[0]
    assert len(result['delegation_slots']) == 3
    assert (result['delegation_slots'][0]['delegator'] == args.creator)
    assert (result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][0]['max_mana'] == 0)
    assert (result['delegation_slots'][1]['delegator'] == "")
    assert (result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][1]['max_mana'] == 0)
    assert (result['delegation_slots'][2]['delegator'] == "")
    assert (result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][2]['max_mana'] == 0)


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

    try:
        rc.delegate_to_pool("initminer", 'alice', '0')
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to delegate 0 when you didn't delegate beforehand"

    rc.delegate_to_pool("alice", 'alice', '0')
    rc_account_before = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account_before['out_delegation_total'] == 0, "out_delegation_total is not 0"
    rc.delegate_to_pool("alice", 'alice', '100')
    rc_account_after = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account_before['rc_manabar']['current_mana'] - rc_account_after['rc_manabar']['current_mana'] > 100, "Current rc was not affected by the delegation to the pool, delta is {}".format(
        rc_account_before['rc_manabar']['current_mana'] - rc_account_after['rc_manabar']['current_mana'])
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
        assert False, "Shouldn't be able to delegate your max_rc"

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
    assert (result['delegation_slots'][0]['delegator'] == "alice")
    assert (result['delegation_slots'][0]["rc_manabar"]["current_mana"] == 50)
    assert (result['delegation_slots'][0]['max_mana'] == 50)
    assert (result['delegation_slots'][1]['delegator'] == "")
    assert (result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][1]['max_mana'] == 0)
    assert (result['delegation_slots'][2]['delegator'] == "")
    assert (result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][2]['max_mana'] == 0)

    rc_account_before = node_client.rpc.find_rc_accounts(["alice"])[0]

    # tx used to use some RC
    rc.set_slot_delegator("null", "rctest2", 2, "rctest2")
    hive_utils.common.wait_n_blocks(args.node_url, 2)

    pool_after = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    assert pool_after['account'] == 'alice'
    assert pool_after['max_rc'] == 100
    assert pool_after['rc_pool_manabar']['current_mana'] < pool['rc_pool_manabar']['current_mana'], "Using rc did not affect the pool"

    rc.delegate_to_pool("alice", 'alice', '0')
    rc_account_after = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account_after['rc_manabar']['current_mana'] - rc_account_before['rc_manabar']['current_mana'] > 75, "Current rc was not affected by the removal of the delegation to the pool"
    assert rc_account_after['max_rc'] - rc_account_before['max_rc'] == 100, "max rc was not affected by the removal of the delegation to the pool"

    result = node_client.rpc.find_rc_accounts(["rctest2"])[0]
    assert len(result['delegation_slots']) == 3
    assert (result['delegation_slots'][0]['delegator'] == "alice")
    assert (result['delegation_slots'][0]["rc_manabar"]["current_mana"] < before_current_mana)
    assert (result['delegation_slots'][0]['max_mana'] == 50)
    assert (result['delegation_slots'][1]['delegator'] == "")
    assert (result['delegation_slots'][1]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][1]['max_mana'] == 0)
    assert (result['delegation_slots'][2]['delegator'] == "null")
    assert (result['delegation_slots'][2]["rc_manabar"]["current_mana"] == 0)
    assert (result['delegation_slots'][2]['max_mana'] == 0)

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
    rc_delegated_to_initminer = str(rc_account['max_rc'] - 200)
    rc.delegate_to_pool("alice", 'initminer', rc_delegated_to_initminer)
    # use RC
    test_utils.create_post(node_client, "alice", "permlink123")
    # delegate 200 when the account's current_mana is less than that
    try:
        rc.delegate_to_pool("alice", 'alice', "200")
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to delegate to a pool if you don't have enough rc"
    rc_account_before = node_client.rpc.find_rc_accounts(["alice"])[0]
    rc.delegate_to_pool("alice", 'alice', "191")
    pool = node_client.rpc.find_rc_delegation_pools(["alice"])[0]
    rc_delegations_to_pools = node_client.rpc.find_rc_delegations("alice")
    assert pool['account'] == 'alice'
    assert pool['max_rc'] == 291
    assert pool['rc_pool_manabar']['current_mana'] == 291
    assert rc_delegations_to_pools[0]['from_account'] == 'alice'
    assert rc_delegations_to_pools[0]['to_pool'] == 'alice'
    assert rc_delegations_to_pools[0]['amount']['amount'] == '191'
    assert rc_delegations_to_pools[1]['from_account'] == 'alice'
    assert rc_delegations_to_pools[1]['to_pool'] == 'initminer'
    assert rc_delegations_to_pools[1]['amount']['amount'] == rc_delegated_to_initminer

    rc_account_after = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account_before['rc_manabar']['current_mana'] > rc_account_after['rc_manabar']['current_mana']
    assert rc_account_before['max_rc'] > rc_account_after['max_rc']

    logger.info("Test removing a pool delegation with no RC")
    rc_account_before = node_client.rpc.find_rc_accounts(["alice"])[0]
    rc.delegate_to_pool("alice", 'initminer', "0")
    rc_account_after = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account_before['rc_manabar']['current_mana'] < rc_account_after['rc_manabar']['current_mana']
    assert rc_account_before['max_rc'] < rc_account_after['max_rc']


def test_delegate_from_pool_full():
    # TODO: overdelegating, bob has a max of 20 rc so he's blocked, but alice has a max of 100 so she can still use the pool
    rc = RC(node_client)
    logger.info("delegate rc from pool full tests")
    rc.delegate_to_pool("initminer", "secondpool", '100')

    try:
        rc.delegate_from_pool("secondpool", "bob", 50)
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to delegate from a pool to an account if the account doesn't have a slot set to the pool"

    rc.set_slot_delegator("secondpool", "bob", 0, args.creator)
    rc.set_slot_delegator("secondpool", "alice", 0, args.creator)
    rc.set_slot_delegator("secondpool", "rctest1", 0, args.creator)
    rc.set_slot_delegator("secondpool", "rctest2", 0, args.creator)

    try:
        rc.delegate_from_pool("secondpool", "bob", 101)
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to delegate more rc than the max rc in the pool"

    try:
        test_utils.create_post(node_client, "bob", "permlink123")
    except Exception:
        pass
    else:
        assert False, "Bob shouldn't be able to create a post with 0 rc"

    logger.info("delegate all the rc in the pool")
    rc.delegate_from_pool("secondpool", "bob", 100)
    pool_before = node_client.rpc.find_rc_delegation_pools(["secondpool"])[0]
    rc_account_before = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert pool_before['account'] == 'secondpool'
    assert pool_before['max_rc'] == 100
    assert pool_before['rc_pool_manabar']['current_mana'] == 100, "pool current mana was affected by the delegation"

    assert len(rc_account_before['delegation_slots']) == 3
    assert (rc_account_before['delegation_slots'][0]['delegator'] == "secondpool")
    assert (rc_account_before['delegation_slots'][0]["rc_manabar"]["current_mana"] == 100)
    assert (rc_account_before['delegation_slots'][0]['max_mana'] == 100)

    # use some RC
    Account("bob", hive_instance=node_client).transfer("bob", 1.000, "TESTS")

    pool_after = node_client.rpc.find_rc_delegation_pools(["secondpool"])[0]
    rc_account_after = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert pool_after['account'] == 'secondpool'
    assert pool_after['max_rc'] == 100
    assert pool_before['rc_pool_manabar']['current_mana'] > pool_after['rc_pool_manabar']['current_mana'], "Pool was not affected by rc consumption"

    assert len(rc_account_before['delegation_slots']) == 3
    assert rc_account_after['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account_after['delegation_slots'][0]['max_mana'] == 100
    assert rc_account_after['delegation_slots'][0]["rc_manabar"]["current_mana"] < rc_account_before['delegation_slots'][0]["rc_manabar"]["current_mana"], "rc account was not affected by rc consumption"
    pool_delta = pool_before['rc_pool_manabar']['current_mana'] - pool_after['rc_pool_manabar']['current_mana']
    account_delegation_delta = rc_account_before['delegation_slots'][0]["rc_manabar"]["current_mana"] - rc_account_after['delegation_slots'][0]["rc_manabar"]["current_mana"]
    assert pool_delta == account_delegation_delta, "The pool rc consumption did not match the delegation manabar consumption"

    logger.info("Test overdelegating to multiple accounts")
    rc.delegate_from_pool("secondpool", "alice", 100)
    rc.delegate_from_pool("secondpool", "rctest1", 100)
    rc.delegate_from_pool("secondpool", "rctest2", 100)

    pool = node_client.rpc.find_rc_delegation_pools(["secondpool"])[0]
    assert pool['account'] == 'secondpool'
    assert pool['max_rc'] == 100
    assert pool['rc_pool_manabar']['current_mana'] == pool_after['rc_pool_manabar']['current_mana'], "pool current mana was affected by the delegation"
    pool_before = pool

    rc_account_alice = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account_alice['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account_alice['delegation_slots'][0]["rc_manabar"]["current_mana"] == 100
    assert rc_account_alice['delegation_slots'][0]['max_mana'] == 100

    rc_account_bob = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert rc_account_bob['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account_bob['delegation_slots'][0]["rc_manabar"]["current_mana"] == rc_account_after['delegation_slots'][0]["rc_manabar"]["current_mana"], "delegating more rc affected bob's current rc"
    assert rc_account_bob['delegation_slots'][0]['max_mana'] == 100

    rc_account_test1 = node_client.rpc.find_rc_accounts(["rctest1"])[0]
    assert rc_account_test1['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account_test1['delegation_slots'][0]["rc_manabar"]["current_mana"] == 100
    assert rc_account_test1['delegation_slots'][0]['max_mana'] == 100

    logger.info("Test consuming from multiple accounts who benefit or not from the pool")
    # consume some RC:
    hive_utils.common.wait_n_blocks(args.node_url, 1)
    Account("bob", hive_instance=node_client).transfer("bob", 1.000, "TESTS")
    hive_utils.common.wait_n_blocks(args.node_url, 1)
    Account("alice", hive_instance=node_client).transfer("alice", 1.000, "TESTS")
    hive_utils.common.wait_n_blocks(args.node_url, 1)

    pool_after = node_client.rpc.find_rc_delegation_pools(["secondpool"])[0]
    assert pool_after['account'] == 'secondpool'
    assert pool_after['max_rc'] == 100
    assert pool_after['rc_pool_manabar']['current_mana'] < pool_before['rc_pool_manabar']['current_mana'], "pool current mana was not affected by the consumption"

    rc_account = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] == rc_account_alice['delegation_slots'][0]["rc_manabar"]["current_mana"], "Rc was pulled from the delegation when alice had 'regular' rc"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] != pool_after['rc_pool_manabar']['current_mana']
    assert rc_account['delegation_slots'][0]['max_mana'] == 100
    rc_account_alice = rc_account

    rc_account = node_client.rpc.find_rc_accounts(["rctest1"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] == 100
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] != pool_after['rc_pool_manabar']['current_mana']
    assert rc_account['delegation_slots'][0]['max_mana'] == 100
    rc_account_test1 = rc_account

    rc_account = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] == pool_after['rc_pool_manabar']['current_mana']
    assert rc_account['delegation_slots'][0]['max_mana'] == 100
    rc_account_bob = rc_account

    pool_before = pool_after

    logger.info("Test consuming from two accounts who benefit from the pool")
    hive_utils.common.wait_n_blocks(args.node_url, 1)
    Account("rctest1", hive_instance=node_client).transfer("rctest1", 1.000, "TESTS")
    hive_utils.common.wait_n_blocks(args.node_url, 1)
    Account("bob", hive_instance=node_client).transfer("bob", 1.000, "TESTS")
    hive_utils.common.wait_n_blocks(args.node_url, 1)

    pool_after = node_client.rpc.find_rc_delegation_pools(["secondpool"])[0]
    assert pool_after['account'] == 'secondpool'
    assert pool_after['max_rc'] == 100
    assert pool_after['rc_pool_manabar']['current_mana'] < pool_before['rc_pool_manabar']['current_mana'], "pool current mana was not affected by the consumption"

    rc_account = node_client.rpc.find_rc_accounts(["alice"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] == rc_account_alice['delegation_slots'][0]["rc_manabar"]["current_mana"], "Rc was pulled from the delegation when alice had 'regular' rc"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] != pool_after['rc_pool_manabar']['current_mana']
    assert rc_account['delegation_slots'][0]['max_mana'] == 100

    rc_account = node_client.rpc.find_rc_accounts(["rctest1"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] < rc_account_test1['delegation_slots'][0]["rc_manabar"]["current_mana"]
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] != pool_after['rc_pool_manabar']['current_mana']
    assert rc_account['delegation_slots'][0]['max_mana'] == 100
    rc_account_test1 = rc_account

    rc_account = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] < rc_account_bob['delegation_slots'][0]["rc_manabar"]["current_mana"]
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] < rc_account_test1['delegation_slots'][0]["rc_manabar"]["current_mana"], "bob's manabar should be lower than rctest1"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] != pool_after['rc_pool_manabar']['current_mana']
    assert rc_account['delegation_slots'][0]['max_mana'] == 100

    logger.info("Test reducing the delegation to an account")
    rc.delegate_from_pool("secondpool", "bob", 10)
    rc_account = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] == 10
    assert rc_account['delegation_slots'][0]['max_mana'] == 10

    logger.info("Test increasing the delegation to an account")
    rc.delegate_from_pool("secondpool", "bob", 20)
    rc_account = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] == 20
    assert rc_account['delegation_slots'][0]['max_mana'] == 20

    logger.info("Test increasing the delegation to an account with a deficit")

    # Use RC
    hive_utils.common.wait_n_blocks(args.node_url, 1)
    Account("bob", hive_instance=node_client).transfer("bob", 1.000, "TESTS")
    hive_utils.common.wait_n_blocks(args.node_url, 1)

    rc.delegate_from_pool("secondpool", "bob", 30)
    rc_account = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] < 30
    assert rc_account['delegation_slots'][0]['max_mana'] == 30

    logger.info("Test removing a delegation")

    rc.delegate_from_pool("secondpool", "bob", 0)
    hive_utils.common.wait_n_blocks(args.node_url, 1)
    try:
        Account("bob", hive_instance=node_client).transfer("bob", 1.000, "TESTS")
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to use rc with no delegation"
    hive_utils.common.wait_n_blocks(args.node_url, 1)

    rc_account = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] == 0
    assert rc_account['delegation_slots'][0]['max_mana'] == 0

    logger.info("Test recreating a delegation that was partially used after deleting it")
    rc.delegate_from_pool("secondpool", "bob", 30)
    rc_account = node_client.rpc.find_rc_accounts(["bob"])[0]
    assert rc_account['delegation_slots'][0]['delegator'] == "secondpool"
    assert rc_account['delegation_slots'][0]["rc_manabar"]["current_mana"] == 30
    assert rc_account['delegation_slots'][0]['max_mana'] == 30


def delegation_rc_exploit_test():
    rc = RC(node_client)
    logger.info("Test exploiting delegations to gain RC")
    rc_account = node_client.rpc.find_rc_accounts(["eve"])[0]
    rc.set_slot_delegator("eve", "eve", 2, "eve")
    # remove all the rc minus 30
    rc.delegate_to_pool("eve", "initminer", str(rc_account['rc_manabar']['current_mana'] - 30))

    rc_account = node_client.rpc.find_rc_accounts(["eve"])[0]
    try:
        rc.delegate_to_pool("eve", "eve", str(rc_account['rc_manabar']['current_mana'] + 1))
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to delegate more than their current mana"

    # delegate most of the rc out
    rc_account = node_client.rpc.find_rc_accounts(["eve"])[0]
    rc.delegate_to_pool("eve", "eve", "20")
    rc.delegate_from_pool("eve", "eve", 20)
    rc_account = node_client.rpc.find_rc_accounts(["eve"])[0]

    # use the mana
    hive_utils.common.wait_n_blocks(args.node_url, 1)
    Account("eve", hive_instance=node_client).transfer("eve", 1.000, "TESTS")
    hive_utils.common.wait_n_blocks(args.node_url, 1)
    Account("eve", hive_instance=node_client).transfer("eve", 2.000, "TESTS")
    hive_utils.common.wait_n_blocks(args.node_url, 1)

    try:
        rc.delegate_from_pool("eve", "eve", 0)
    except Exception:
        pass
    else:
        assert False, "Shouldn't be able to undelegate removing its only source of rc"

    rc_account_before = node_client.rpc.find_rc_accounts(["eve"])[0]
    pool_before = node_client.rpc.find_rc_delegation_pools(["eve"])[0]
    rc.delegate_to_pool("eve", "eve", "0")
    rc_account_after = node_client.rpc.find_rc_accounts(["eve"])[0]
    pool_after = node_client.rpc.find_rc_delegation_pools(["eve"])[0]
    # we know the price of an rc op is 3 so the new current mana should be rc_account_before.current + pool_before.current_mana - 3
    assert rc_account_after['rc_manabar']['current_mana'] == rc_account_before['rc_manabar']['current_mana'] + pool_before['rc_pool_manabar']['current_mana'] - 3
    assert rc_account_before['max_rc'] == 10
    assert rc_account_after['max_rc'] == 30
    assert pool_after['account'] == 'eve'
    assert pool_after['max_rc'] == 0
    assert pool_after['rc_pool_manabar']['current_mana'] == 0

    assert rc_account_after['delegation_slots'][2]['delegator'] == "eve"
    assert rc_account_after['delegation_slots'][2]['max_mana'] == 20
    assert rc_account_before['delegation_slots'][2]["rc_manabar"]["current_mana"] == rc_account_after['delegation_slots'][2]["rc_manabar"]["current_mana"]


if __name__ == '__main__':
    logger.info("Performing RC tests")
    from beem.rc import RC
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("creator", help="Account to create test accounts with")
    parser.add_argument("wif", help="Private key for creator account")
    parser.add_argument("--node-url", dest="node_url", default="http://127.0.0.1:8090", help="Url of working hive node")
    parser.add_argument("--run-hived", dest="hived_path", help="Path to hived executable. Warning: using this option will erase contents of selected hived working directory.")
    parser.add_argument("--working-dir", dest="hived_working_dir", default="/tmp/hived-data/", help="Path to hived working directory")
    parser.add_argument("--config-path", dest="hived_config_path", default="../../hive_utils/resources/config.ini.in", help="Path to source config.ini file")
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
        {"name": "pool", "private_key": "5KgfcV9bgEen3v9mxkoGw6Rhuf2giDRZTHZjzwisjkrpF4FUh3N", "public_key": "TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn"},
        {"name": "secondpool", "private_key": "5JQtsuY4E6nqVtPfemEq47nCpZ8HD3jWpevnVPNQfHGBr3jwxdy", "public_key": "TST4tvU3jn3d2Qnvk3hJ9sE2Mufo6TFa8bwJGmLAE25WxUPfN6oxc"},
        {"name": "alice", "private_key": "5KgxYhw6yK9QrLKQGAxRqauGuGycMVF1QNbE43Sr3E28Lo54cYq", "public_key": "TST73QDXtjvmc97qGDeCmA8Why7JHnWjzkpEuW6Htr56R2DJ6d58q"},
        {"name": "bob", "private_key": "5JfLZHtGWBP8NjNkP5xkxf6GvvMaY4ZGanSzm7qSmDCrQFq22uy", "public_key": "TST6NrizgPtefvuQV1g43Q2mKJKy1YAQMyhQJhRm4g4PtEtGvhn4N"},
        {"name": "rctest1", "private_key": "5KQeu7SdzxT1DiUzv7jaqwkwv1V8Fi7N8NBZtHugWYXqVFH1AFa", "public_key": "TST8VfiahQsfS1TLcnBfp4NNfdw67uWweYbbUXymbNiDXVDrzUs7J"},
        {"name": "rctest2", "private_key": "5JcQCDVnu7v7PWpsKUD8TcQqhB1v2WvSbfhK8JzDywdKEWBddNX", "public_key": "TST6rb6ZbcvqufSHbpt6gAWT4YniDHLnMvU7QoEZbRvNG31USzYnX"},
        {"name": "rctest3", "private_key": "5J1QbAFcF9Xp9gVtvNMXbmzA16Cy15nKjCCZiVQa2ZxMmhZQnBd", "public_key": "TST7vFHFVKpH9R37e6M6n7CkV6CAQZFR41Gc2f4nxNu7EW9347shb"},
        {"name": "eve", "private_key": "5Jb6QtKKwoxf2aQBHTxP8zSPcAiduwZutRYezfNFGJhoG11skUx", "public_key": "TST5zjb6WQuLtbngGNFyGspLFScA5j41X2gGSJaKE8hmry5an9q6N"},
        {"name": "badpool", "private_key": "5HubGcDgerXgebpqyREHxNJqotUrycYPcbSWznnaJ8UgXqoZ35U", "public_key": "TST5zE21bgn1B7gzXtXnfY3LFcp67SD1rd8eNttGZhgQXEeBsCQUV"},
    ]
    account_names = [v['name'] for v in accounts]

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
            node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)
            test_utils.create_accounts(node_client, args.creator, accounts)
            test_utils.transfer_to_vesting(node_client, args.creator, 'pool', "300.000", "TESTS")
            test_utils.transfer_to_vesting(node_client, args.creator, 'secondpool', "300.000", "TESTS")
            test_utils.transfer_to_vesting(node_client, args.creator, 'alice', "300.000", "TESTS")
            test_utils.transfer_to_vesting(node_client, args.creator, 'eve', ".100", "TESTS")
            Account(args.creator, hive_instance=node_client).transfer("rctest1", 100.000, "TESTS")
            Account(args.creator, hive_instance=node_client).transfer("bob", 100.000, "TESTS")
            Account(args.creator, hive_instance=node_client).transfer("alice", 100.000, "TESTS")
            Account(args.creator, hive_instance=node_client).transfer("rctest2", 100.000, "TESTS")
            Account(args.creator, hive_instance=node_client).transfer("eve", 100.000, "TESTS")

            test_delegate_to_pool()
            test_set_delegator_slot()
            test_delegate_rc()
            test_set_slot_remove_rc()
            test_set_slot()
            test_delegate_to_pool_full()
            test_delegate_from_pool_full()
            delegation_rc_exploit_test()
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
        sys.exit(return_code)

