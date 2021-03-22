from network import Network
from key_generator import KeyGenerator
from witness import Witness


import json
import os
import sys
import time
import concurrent

sys.path.append("../../../tests_api")
from jsonsocket import hived_call
from hive.steem.client import SteemClient

# TODO: Remove dependency from cli_wallet/tests directory.
#       This modules [utils.logger] should be somewhere higher.
sys.path.append("../cli_wallet/tests")
from utils.logger import log, init_logger

CONCURRENCY = 5


def set_password(_url):
    log.info("Call to set_password")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "set_password",
      "params": ["testpassword"]
      } ), "utf-8" ) + b"\r\n"

    status, response = hived_call(_url, data=request)
    return status, response

def unlock(_url):
    log.info("Call to unlock")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "unlock",
      "params": ["testpassword"]
      } ), "utf-8" ) + b"\r\n"

    status, response = hived_call(_url, data=request)
    return status, response


def import_key(_url, k):
    log.info("Call to import_key")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "import_key",
      "params": [k]
      } ), "utf-8" ) + b"\r\n"

    status, response = hived_call(_url, data=request)
    return status, response

def checked_hived_call(_url, data):
  status, response = hived_call(_url, data)
  if status == False or response is None or "result" not in response:
    log.error("Request failed: {0} with response {1}".format(str(data), str(response)))
    raise Exception("Broken response for request {0}: {1}".format(str(data), str(response)))

  return status, response

def wallet_call(_url, data):
  unlock(_url)
  status, response = checked_hived_call(_url, data)

  return status, response

def create_account(_url, _creator, account_name):
    account = Witness(account_name)

    _name = account_name
    _pubkey = account.public_key
    _privkey = account.private_key

    log.info("Call to create account {0} {1}".format(str(_creator), str(_name)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "create_account_with_keys",
      "params": [_creator, _name, "", _pubkey, _pubkey, _pubkey, _pubkey, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)
    status, response = import_key(_url, _privkey)

def transfer(_url, _sender, _receiver, _amount, _memo):
    log.info("Attempting to transfer from {0} to {1}, amount: {2}".format(str(_sender), str(_receiver), str(_amount)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "transfer",
      "params": [_sender, _receiver, _amount, _memo, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def transfer_to_vesting(_url, _sender, _receiver, _amount):
    log.info("Attempting to transfer_to_vesting from {0} to {1}, amount: {2}".format(str(_sender), str(_receiver), str(_amount)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "transfer_to_vesting",
      "params": [_sender, _receiver, _amount, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def set_voting_proxy(_account, _proxy, _url):
    log.info("Attempting to set account `{0} as proxy to {1}".format(str(_proxy), str(_account)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "set_voting_proxy",
      "params": [_account, _proxy, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def vote_for_witness(_account, _witness, _approve, _url):
  if(_approve):
    log.info("Account `{0} votes for wittness {1}".format(str(_account), str(_witness)))
  else:
    log.info("Account `{0} revoke its votes for wittness {1}".format(str(_account), str(_witness)))

  request = bytes( json.dumps( {
    "jsonrpc": "2.0",
    "id": 0,
    "method": "vote_for_witness",
    "params": [_account, _witness, _approve, 1]
    } ), "utf-8" ) + b"\r\n"

  status, response = wallet_call(_url, data=request)

def vote_for_witnesses(_account, _witnesses, _approve, _url):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  for w in _witnesses:
    if(isinstance(w, str)):
      account_name = w
    else:
      account_name = w["account_name"]
    future = executor.submit(vote_for_witness, _account, account_name, 1, _url)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

def register_witness(_url, _account_name, _witness_url, _block_signing_public_key):
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "update_witness",
      "params": [_account_name, _witness_url, _block_signing_public_key, {"account_creation_fee": "3.000 TESTS","maximum_block_size": 65536,"sbd_interest_rate": 0}, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def unregister_witness(_url, _account_name):
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "update_witness",
      "params": [_account_name, "https://heaven.net", "STM1111111111111111111111111111111114T1Anm", {"account_creation_fee": "3.000 TESTS","maximum_block_size": 65536,"sbd_interest_rate": 0}, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def info(_url):
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "info",
      "params": []
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

    return response["result"]

def get_amount(_asset):
  amount, symbol = _asset.split(" ")
  return float(amount)

def self_vote(_witnesses, _url):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  for w in _witnesses:
    if(isinstance(w, str)):
      account_name = w
    else:
      account_name = w["account_name"]
    future = executor.submit(vote_for_witness, account_name, account_name, 1, _url)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

def prepare_accounts(_accounts, _url):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  log.info("Attempting to create {0} accounts".format(len(_accounts)))
  for account in _accounts:
    future = executor.submit(create_account, _url, "initminer", account)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

def configure_initial_vesting(_accounts, _tests, _url):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  log.info("Attempting to prepare {0} of witnesses".format(str(len(_accounts))))
  for account_name in _accounts:
    future = executor.submit(transfer_to_vesting, _url, "initminer", account_name, _tests)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

def prepare_witnesses(_witnesses, _url):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  log.info("Attempting to prepare {0} of witnesses".format(str(len(_witnesses))))
  for account_name in _witnesses:
    witness = Witness(account_name)
    pub_key = witness.public_key
    future = executor.submit(register_witness, _url, account_name, "https://" + account_name + ".net", pub_key)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

def synchronize_balances(_accounts, _url, _mainNetUrl):
  log.info("Attempting to synchronize balances of {0} accounts".format(str(len(_accounts))))

  accountList = []

  for a in _accounts:
    accountList.append(a["account_name"])

  client = Client(_mainNetUrl)
  accounts = client.get_accounts(accountList)

  log.info("Successfully retrieved info for {0} accounts".format(str(len(accounts))))

  for a in accounts:
    name = a["name"]
    vests = a["vesting_shares"]
    steem = a["balance"]
    sbd = a["sbd_balance"]

    log.info("Account {0} balances: `{1}', `{2}', `{3}'".format(str(name), str(steem), str(sbd), str(vests)))

    steem_amount, steem_symbol = steem.split(" ")

    if(float(steem_amount) > 0):
      tests = steem.replace("STEEM", "TESTS")
      transfer(_url, "initminer", name, tests, "initial balance sync")

    sbd_amount, sbd_symbol = sbd.split(" ")
    if(float(sbd_amount) > 0):
      tbd = sbd.replace("SBD", "TBD")
      transfer(_url, "initminer", name, tbd, "initial balance sync")

def list_accounts(url):
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "list_accounts",
      "params": ["", 100]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(url, data=request)
    print(response)

def list_top_witnesses(_url):
    start_object = [200277786075957257, ""]
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "database_api.list_witnesses",
      "params": {
        "limit": 100,
        "order": "by_vote_name",
        "start": start_object
      }
    } ), "utf-8" ) + b"\r\n"

    status, response = checked_hived_call(_url, data=request)
    return response["result"]["witnesses"]

def print_top_witnesses(sockpuppets, witnesses, api_node_url):
  sockpuppets_set = set(sockpuppets)
  witnesses_set = set(witnesses)

  top_witnesses = list_top_witnesses(api_node_url)
  position = 1
  for w in top_witnesses:
    owner = w["owner"]
    group = "U"
    if owner in sockpuppets_set:
      group = "C"
    elif owner in witnesses_set:
      group = "W"

    log.info("Witness # {0:2d}, group: {1}, name: `{2}', votes: {3}".format(position, group, w["owner"], w["votes"]))
    position = position + 1


if __name__ == "__main__":
    try:
        error = False
        init_logger(os.path.abspath(__file__))

        Witness.key_generator = KeyGenerator('../../../../build/programs/util/get_dev_key')

        sockpuppet_names = [f'sockpuppet{i}' for i in range(20)]
        witness_names = [f'witness{i}' for i in range(9)]
        good_wtns_names = [f'good-wtns-{i}' for i in range(14)]
        steemit_acc_names = [f'steemit-acc-{i}' for i in range(13)]

        # Create first network
        alpha_net = Network('Alpha', port_range=range(51000, 52000))
        alpha_net.set_directory('experimental_network')
        alpha_net.set_hived_executable_file_path('../../../../build/programs/hived/hived')
        alpha_net.set_wallet_executable_file_path('../../../../build/programs/cli_wallet/cli_wallet')

        init_node = alpha_net.add_node('InitNode')
        node0 = alpha_net.add_node('Node0')
        node1 = alpha_net.add_node('Node1')
        node2 = alpha_net.add_node('Node2')

        # Create second network
        beta_net = Network('Beta', port_range=range(52000, 53000))
        beta_net.set_directory('experimental_network')
        beta_net.set_hived_executable_file_path('../../../../build/programs/hived/hived')
        beta_net.set_wallet_executable_file_path('../../../../build/programs/cli_wallet/cli_wallet')

        beta_net.add_node('Node0')
        beta_net.add_node('Node1')
        api_node = beta_net.add_node('Node2')

        # Create witnesses
        init_node.set_witness('initminer')
        for name in sockpuppet_names:
            init_node.set_witness(name)

        for name in witness_names:
            node0.set_witness(name)

        for name in good_wtns_names:
            node1.set_witness(name)

        for name in steemit_acc_names:
            node2.set_witness(name)

        # Run
        alpha_net.connect_with(beta_net)

        alpha_net.run()
        beta_net.run()

        wallet = alpha_net.attach_wallet()

        time.sleep(3)  # Wait for wallet to start

        # Run original test script
        wallet_url = f'http://127.0.0.1:{wallet.http_server_port}'
        api_node_url = f'http://{api_node.get_webserver_http_endpoints()[0]}'

        set_password(wallet_url)
        unlock(wallet_url)
        import_key(wallet_url, Witness('initminer').private_key)

        print_top_witnesses(sockpuppet_names, witness_names, api_node_url)

        list_accounts(wallet_url)

        prepare_accounts(steemit_acc_names, wallet_url)
        prepare_accounts(['steemit-proxy'], wallet_url)

        prepare_accounts(sockpuppet_names, wallet_url)
        prepare_accounts(witness_names, wallet_url)

        configure_initial_vesting(steemit_acc_names, "1000000.000 TESTS", wallet_url)
        configure_initial_vesting(witness_names, "12000000.000 TESTS", wallet_url)

        configure_initial_vesting(['steemit-proxy'], "10.000 TESTS", wallet_url)
        configure_initial_vesting(sockpuppet_names, "10.000 TESTS", wallet_url)
        configure_initial_vesting(witness_names, "1000000.000 TESTS", wallet_url)

        prepare_witnesses(sockpuppet_names, wallet_url)
        prepare_witnesses(witness_names, wallet_url)

        self_vote(witness_names, wallet_url)

        print("Witness state before voting of steemit-proxy proxy")
        print_top_witnesses(sockpuppet_names, witness_names, api_node_url)

        vote_for_witnesses("steemit-proxy", sockpuppet_names, 1, wallet_url)

        print("Witness state after voting of steemit-proxy proxy")
        print_top_witnesses(sockpuppet_names, witness_names, api_node_url)

        list_accounts(wallet_url)

        print(60 * '=')
        print(' Network successfully prepared')
        print(60 * '=')

        input('Press enter to disconnect networks')
        alpha_net.disconnect_from(beta_net)

        print('Disconnected')
        while True:
            pass

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

