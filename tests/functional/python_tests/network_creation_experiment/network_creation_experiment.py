from pathlib import Path
import time

from network import Network
from node import Node
from wallet import Wallet

hived_executable_file_path = Path('../../../../build/programs/hived/hived').absolute()
wallet_executable_file_path = Path('../../../../build/programs/cli_wallet/cli_wallet').absolute()
main_directory = Path('experimental_network/')
wallets = []


def create_wallet():
    wallet = Wallet(directory=main_directory / 'Wallet')
    wallet.set_executable_file_path(wallet_executable_file_path)
    wallets.append(wallet)
    return wallet


if __name__ == "__main__":
    net = Network('TestNetwork')
    net.set_hived_executable_file_path('../../../../build/programs/hived/hived')
    net.set_wallet_executable_file_path('../../../../build/programs/cli_wallet/cli_wallet')

    init_node = net.add_node('InitNode')
    net.add_node('TestNode0')
    net.add_node('TestNode1')
    net.add_node('TestNode2')

    net.run()

    wallet = net.attach_wallet()

    while True:
        pass

    ####################################
    #   Code below should be removed   #
    ####################################

    print('Script is run in', Path('.').absolute())

    if main_directory.exists():
        print('Clear whole directory', main_directory)
        rmtree(main_directory)

    main_directory.mkdir()

    node_names = ['TestNode0', 'TestNode1']
    witnesses = {
        'TestNode0': 'initminer',
        'TestNode1': 'other-witness',
    }
    witnesses_keys = {
        'TestNode0': '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n',
        'TestNode1': '5Kji6CXkau7xm1cegSVHrL9dfH6gXC3FfN5NxRqA2KevQGziKn8',
    }
    endpoints = {
        'TestNode0': '0.0.0.0:3001',
        'TestNode1': '0.0.0.0:41001',
    }
    seed_nodes = {
        'TestNode0': '127.0.0.1:3101',
        'TestNode1': '127.0.0.1:41000',
    }
    webserver_http_endpoint = {
        'TestNode0': '0.0.0.0:3903',
        'TestNode1': '0.0.0.0:4003',
    }
    webserver_ws_endpoint = {
        'TestNode0': '0.0.0.0:13904',
        'TestNode1': '0.0.0.0:4004',
    }

    for node_name in node_names:
        node = create_node(name=node_name)
        node.set_witness(witnesses[node_name], witnesses_keys[node_name])

        node.config.add_entry(
            'p2p-endpoint',
            endpoints[node_name],
            'The local IP address and port to listen for incoming connections'
        )

        node.config.add_entry(
            'p2p-seed-node',
            seed_nodes[node_name],
            'The IP address and port of a remote peer to sync with'
        )

        node.config.add_entry(
            'webserver-http-endpoint',
            webserver_http_endpoint[node_name],
            'Local http endpoint for webserver requests'
        )

        node.config.add_entry(
            'webserver-ws-endpoint',
            webserver_ws_endpoint[node_name],
            'Local websocket endpoint for webserver requests'
        )

    for node in nodes:
        node.run()

    time.sleep(2)

    # ./cli_wallet --chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39 -s ws://0.0.0.0:3903 -d -H 0.0.0.0:3904 --rpc-http-allowip 192.168.10.10 --rpc-http-allowip=127.0.0.1

    wallet = create_wallet()
    wallet.connect_to(nodes[0])
    wallet.run()

    time.sleep(3)
    exit(0)

    while True:
        pass
