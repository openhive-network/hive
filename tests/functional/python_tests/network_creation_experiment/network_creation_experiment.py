from test_library import Network, KeyGenerator, Witness


import os
import sys
import time
import concurrent.futures
import random

# TODO: Remove dependency from cli_wallet/tests directory.
#       This modules [utils.logger] should be somewhere higher.
sys.path.append("../cli_wallet/tests")
from utils.logger import log, init_logger

CONCURRENCY = None


def register_witness(wallet, _account_name, _witness_url, _block_signing_public_key):
    wallet.api.update_witness(
        _account_name,
        _witness_url,
        _block_signing_public_key,
        {"account_creation_fee": "3.000 TESTS", "maximum_block_size": 65536, "sbd_interest_rate": 0}
    )

def self_vote(_witnesses, wallet):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  for w in _witnesses:
    if(isinstance(w, str)):
      account_name = w
    else:
      account_name = w["account_name"]
    future = executor.submit(wallet.api.vote_for_witness, account_name, account_name, 1)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

def prepare_accounts(_accounts, wallet):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  log.info("Attempting to create {0} accounts".format(len(_accounts)))
  for account in _accounts:
    future = executor.submit(wallet.api.create_account_with_keys, 'initminer', account)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

  fs = []
  for account in _accounts:
    future = executor.submit(wallet.api.import_key, Witness(account).private_key)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

def configure_initial_vesting(_accounts, a, b, _tests, wallet):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  log.info("Configuring initial vesting for {0} of witnesses".format(str(len(_accounts))))
  for account_name in _accounts:
    value = random.randint(a, b)
    amount = str(value) + ".000 " + _tests
    future = executor.submit(wallet.api.transfer_to_vesting, "initminer", account_name, amount)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

def prepare_witnesses(_witnesses, wallet):
  executor = concurrent.futures.ThreadPoolExecutor(max_workers=CONCURRENCY)
  fs = []
  log.info("Attempting to prepare {0} of witnesses".format(str(len(_witnesses))))
  for account_name in _witnesses:
    witness = Witness(account_name)
    pub_key = witness.public_key
    future = executor.submit(register_witness, wallet, account_name, "https://" + account_name + ".net", pub_key)
    fs.append(future)
  res = concurrent.futures.wait(fs, timeout=None, return_when=concurrent.futures.ALL_COMPLETED)

def list_top_witnesses(node):
    start_object = [200277786075957257, '']
    response = node.api.database.list_witnesses(100, 'by_vote_name', start_object)
    return response["result"]["witnesses"]

def print_top_witnesses(witnesses, node):
  witnesses_set = set(witnesses)

  top_witnesses = list_top_witnesses(node)
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
        wallet.api.set_password()
        wallet.api.unlock()
        wallet.api.import_key(Witness('initminer').private_key)

        all_witnesses = alpha_witness_names + beta_witness_names
        random.shuffle(all_witnesses)

        print("Witness state before voting")
        print_top_witnesses(all_witnesses, api_node)
        print(wallet.api.list_accounts()[1])

        prepare_accounts(all_witnesses, wallet)
        configure_initial_vesting(all_witnesses, 500, 1000, "TESTS", wallet)
        prepare_witnesses(all_witnesses, wallet)
        self_vote(all_witnesses, wallet)

        print("Witness state after voting")
        print_top_witnesses(all_witnesses, api_node)
        print(wallet.api.list_accounts()[1])

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

