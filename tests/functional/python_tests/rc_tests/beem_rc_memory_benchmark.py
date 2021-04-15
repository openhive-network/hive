#!/usr/bin/python3

import sys

sys.path.append("../../")
import hive_utils
import requests
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


def get_valid_hive_account_name():
    user_name = list("baa")
    http_url = args.server_http_endpoint
    while True:
        params = {"jsonrpc":"2.0", "method":"condenser_api.get_accounts", "params":[["".join(user_name)]], "id":1}
        resp = requests.post(http_url, json=params)
        data = resp.json()
        if "result" in data:
            if len(data["result"]) == 0:
                return ''.join(user_name)
        if ord(user_name[0]) >= ord('z'):
            for i, _ in enumerate("".join(user_name)):
                if user_name[i] >= 'z':
                    user_name[i] = 'a'
                    continue
                user_name[i] = chr(ord(user_name[i]) + 1)
                break
        else:
            user_name[0] = chr(ord(user_name[0]) + 1)
        if len(set(user_name)) == 1 and user_name[0] == 'z':
            break


if __name__ == '__main__':
    logger.info("/!\ this test it not to be executed as part of the pipeline, this is just ment to benchmark memory usage /!\ ")
    import argparse
    from beem.transactionbuilder import TransactionBuilder
    from beembase import operations
    from beem.instance import set_shared_blockchain_instance
    from beem.rc import RC

    parser = argparse.ArgumentParser()
    parser.add_argument("creator", help="Account to create test accounts with")
    parser.add_argument("wif", help="Private key for creator account")
    parser.add_argument("--node-url", dest="node_url", default="http://127.0.0.1:8090", help="Url of working hive node")
    parser.add_argument('--server-http-endpoint', dest="server_http_endpoint", help = "Set server endpoint [=http://127.0.0.1:8090]", default = "http://127.0.0.1:8090")
    parser.add_argument("--run-hived", dest="hived_path", help="Path to hived executable. Warning: using this option will erase contents of selected hived working directory.")
    parser.add_argument("--working-dir", dest="hived_working_dir", default="/tmp/hived-data/", help="Path to hived working directory")
    parser.add_argument("--config-path", dest="hived_config_path", default="../../hive_utils/resources/config.ini.in", help="Path to source config.ini file")

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

    keys = [wif]

    if node is not None:
        node.run_hive_node(["--enable-stale-production"])

    if node is None or node.is_running():
        node_client = Hive(node=[node_url], no_broadcast=False, keys=keys)

        # create the account to be the account_auth with the smallest name possible, reducing the op size
        params = {"jsonrpc":"2.0", "method":"condenser_api.get_accounts", "params":[["aaa"]], "id":1}
        resp = requests.post(args.server_http_endpoint, json=params)
        data = resp.json()
        if "result" in data:
            if len(data["result"]) == 0:
                node_client.create_account("aaa",
                                           store_keys = False,
                                           creator=args.creator,
                                           owner_key='TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn',
                                           active_key='TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn',
                                           posting_key='TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn',
                                           memo_key='TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn',
                                           storekeys=False,
                                           asset='TESTS'
                                           )

        set_shared_blockchain_instance(node_client)
        start_account_name = get_valid_hive_account_name()
        account_name = start_account_name
        tx = TransactionBuilder()
        max_account_creation_per_op=1200
        account_nb=1000000
        name_index = 0
        while name_index < account_nb:
            for k in range(max_account_creation_per_op):
                op = operations.Account_create(**{"fee": '0.000 TESTS',
                                                  "creator": args.creator,
                                                  "new_account_name": account_name,
                                                  'owner': {'account_auths': [["aaa", 1]],
                                                            'key_auths': [],
                                                            'weight_threshold': 1},
                                                  'active': { 'account_auths': [["aaa", 1]],
                                                             'key_auths': [],
                                                             'weight_threshold': 1},
                                                  'posting': { 'account_auths': [["aaa", 1]],
                                                              'key_auths': [],
                                                              'weight_threshold': 1},
                                                  "memo_key": 'TST5gQPYm5bs9dRPHpqBy6dU32M8FcoKYFdF4YWEChUarc9FdYHzn',
                                                  "json_metadata": {},
                                                  "prefix": 'TST',
                                                  })
                tx.appendOps(op)
                account_name = start_account_name + str(name_index)
                name_index += 1
            tx.appendWif(args.wif)
            tx.sign()
            tx.broadcast()
            logger.info("number of account created: {}".format(name_index))

        # TODO: get memory of hived here (currently done via breakpoints)
        rc = RC(node_client)
        logger.info("Delegating to the main pool")
        rc.delegate_to_pool(args.creator, args.creator, '50000')
        name_index = 0
        account_name = start_account_name
        max_delegation_per_op=770
        logger.info("Delegating to every account")
        tx = TransactionBuilder()
        while name_index < account_nb:
            for k in range(max_delegation_per_op):
                json_op = {
                    "required_auths": [args.creator],
                    "required_posting_auths": [],
                    "id": "rc",
                    "json": [
                        'delegate_drc_from_pool', {
                            'from_pool': args.creator,
                            'to_account': account_name,
                            'asset_symbol': {'nai': "@@000000037", 'decimals': 6},
                            "drc_max_mana": 50,
                        }
                    ]
                }
                tx.appendOps(operations.Custom_json(json_op))
                account_name = start_account_name + str(name_index)
                name_index += 1
            tx.appendWif(args.wif)
            tx.sign()
            tx.broadcast()
            logger.info("number of account delegated to: {}".format(name_index))
        # TODO: get memory of hived here (currently done via breakpoints)
    else:
        raise Exception("no node detected")

