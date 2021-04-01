from test_library import Network, KeyGenerator, Witness


import json
import os
import sys
import time
import concurrent.futures
import random

sys.path.append("../../../tests_api")
from jsonsocket import hived_call
from hive.steem.client import SteemClient

# TODO: Remove dependency from cli_wallet/tests directory.
#       This modules [utils.logger] should be somewhere higher.
sys.path.append("../cli_wallet/tests")
from utils.logger import log, init_logger

CONCURRENCY = None


def set_password(_url):
    log.debug("Call to set_password")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "set_password",
      "params": ["testpassword"]
      } ), "utf-8" ) + b"\r\n"

    status, response = hived_call(_url, data=request)
    return status, response

def unlock(_url):
    log.debug("Call to unlock")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "unlock",
      "params": ["testpassword"]
      } ), "utf-8" ) + b"\r\n"

    status, response = hived_call(_url, data=request)
    return status, response


def import_key(_url, k):
    log.debug("Call to import_key")
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

def configure_initial_vesting(_accounts, a, b, _tests, wallet):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  log.info("Configuring initial vesting for {0} of witnesses".format(str(len(_accounts))))
  for account_name in _accounts:
    value = random.randint(a, b)
    amount = str(value) + ".000 " + _tests
    future = executor.submit(wallet.transfer_to_vesting, "initminer", account_name, amount)
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

def print_top_witnesses(witnesses, node):
  while not node.is_http_listening():
    time.sleep(1)

  if not node.get_webserver_http_endpoints():
    raise Exception("Node hasn't set any http endpoint")

  api_node_url = node.get_webserver_http_endpoints()[0]

  witnesses_set = set(witnesses)

  top_witnesses = list_top_witnesses(api_node_url)
  position = 1
  for w in top_witnesses:
    owner = w["owner"]
    group = "U"
    if owner in witnesses_set:
      group = "W"

    log.info("Witness # {0:2d}, group: {1}, name: `{2}', votes: {3}".format(position, group, w["owner"], w["votes"]))
    position = position + 1


if __name__ == "__main__":
    try:
        error = False
        init_logger(os.path.abspath(__file__))

        Witness.key_generator = KeyGenerator('../../../../build/programs/util/get_dev_key')

        alpha_witness_names = [f'witness{i}-alpha' for i in range(20)]
        beta_witness_names = [f'witness{i}-beta' for i in range(20)]

        # Create first network
        alpha_net = Network('Alpha', port_range=range(51000, 52000))
        alpha_net.set_directory('experimental_network')
        alpha_net.set_hived_executable_file_path('../../../../build/programs/hived/hived')
        alpha_net.set_wallet_executable_file_path('../../../../build/programs/cli_wallet/cli_wallet')

        init_node = alpha_net.add_node('InitNode')
        alpha_node0 = alpha_net.add_node('Node0')
        alpha_node1 = alpha_net.add_node('Node1')
        alpha_node2 = alpha_net.add_node('Node2')

        # Create second network
        beta_net = Network('Beta', port_range=range(52000, 53000))
        beta_net.set_directory('experimental_network')
        beta_net.set_hived_executable_file_path('../../../../build/programs/hived/hived')
        beta_net.set_wallet_executable_file_path('../../../../build/programs/cli_wallet/cli_wallet')

        beta_node0 = beta_net.add_node('Node0')
        beta_node1 = beta_net.add_node('Node1')
        api_node = beta_net.add_node('Node2')

        # Create witnesses
        init_node.set_witness('initminer')
        alpha_witness_nodes = [alpha_node0, alpha_node1, alpha_node2]
        for name in alpha_witness_names:
            node =  random.choice(alpha_witness_nodes)
            node.set_witness(name)

        beta_witness_nodes = [beta_node0, beta_node1]
        for name in beta_witness_names:
            node =  random.choice(beta_witness_nodes)
            node.set_witness(name)

        # Run
        alpha_net.connect_with(beta_net)

        alpha_net.run()
        beta_net.run()

        wallet = alpha_net.attach_wallet()

        time.sleep(3)  # Wait for wallet to start

        # Run original test script
        wallet_url = f'http://127.0.0.1:{wallet.http_server_port}'

        wallet.set_password()
        wallet.unlock()
        wallet.import_key(Witness('initminer').private_key)

        all_witnesses = alpha_witness_names + beta_witness_names
        random.shuffle(all_witnesses)

        print("Witness state before voting")
        print_top_witnesses(all_witnesses, api_node)
        print(wallet.list_accounts()[1])

        prepare_accounts(all_witnesses, wallet_url)
        configure_initial_vesting(all_witnesses, 500, 1000, "TESTS", wallet)
        prepare_witnesses(all_witnesses, wallet_url)
        self_vote(all_witnesses, wallet_url)

        print("Witness state after voting")
        print_top_witnesses(all_witnesses, api_node)
        print(wallet.list_accounts()[1])

        print("Waiting for network synchronization...")
        alpha_net.wait_for_synchronization_of_all_nodes()
        beta_net.wait_for_synchronization_of_all_nodes()

        print(60 * '=')
        print(' Network successfully prepared')
        print(60 * '=')

        while True:
          input('Press enter to disconnect networks')
          alpha_net.disconnect_from(beta_net)
          print('Disconnected')

          input('Press enter to reconnect networks')
          alpha_net.connect_with(beta_net)
          print('Reconnected')

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

