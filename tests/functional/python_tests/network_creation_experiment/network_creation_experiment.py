from network import Network
from key_generator import KeyGenerator
from witness import Witness


import json
import os
import sys
import time

sys.path.append("../../../tests_api")
from jsonsocket import hived_call
from hive.steem.client import SteemClient

# TODO: Remove dependency from cli_wallet/tests directory.
#       This modules [utils.logger] should be somewhere higher.
sys.path.append("../cli_wallet/tests")
from utils.logger import log, init_logger


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
        init_node.set_witness('initminer')
        for name in sockpuppet_names:
            init_node.set_witness(name)

        node0 = alpha_net.add_node('Node0')
        for name in witness_names:
            node0.set_witness(name)

        node1 = alpha_net.add_node('Node1')
        for name in good_wtns_names:
            node1.set_witness(name)

        node2 = alpha_net.add_node('Node2')
        for name in steemit_acc_names:
            node2.set_witness(name)

        # Create second network
        beta_net = Network('Beta', port_range=range(52000, 53000))
        beta_net.set_directory('experimental_network')
        beta_net.set_hived_executable_file_path('../../../../build/programs/hived/hived')
        beta_net.set_wallet_executable_file_path('../../../../build/programs/cli_wallet/cli_wallet')

        beta_net.add_node('Node0')
        beta_net.add_node('Node1')
        beta_net.add_node('Node2')

        # Run
        alpha_net.connect_with(beta_net)

        alpha_net.run()
        beta_net.run()

        wallet = alpha_net.attach_wallet()

        time.sleep(3)  # Wait for wallet to start

        wallet_url = f'http://127.0.0.1:{wallet.http_server_port}'
        set_password(wallet_url)
        unlock(wallet_url)

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

