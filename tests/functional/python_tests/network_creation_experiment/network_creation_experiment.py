from network import Network
from key_generator import KeyGenerator
from witness import Witness


if __name__ == "__main__":
    Witness.key_generator = KeyGenerator('../../../../build/programs/util/get_dev_key')

    # Create first network
    alpha_net = Network('Alpha', port_range=range(51000, 52000))
    alpha_net.set_directory('experimental_network')
    alpha_net.set_hived_executable_file_path('../../../../build/programs/hived/hived')
    alpha_net.set_wallet_executable_file_path('../../../../build/programs/cli_wallet/cli_wallet')

    init_node = alpha_net.add_node('InitNode')
    init_node.set_witness('initminer')
    for i in range(20):
        init_node.set_witness(f'sockpuppet{i}')

    node0 = alpha_net.add_node('Node0')
    for i in range(9):
        node0.set_witness(f'witness{i}')

    node1 = alpha_net.add_node('Node1')
    for i in range(14):
        node1.set_witness(f'good-wtns-{i}')

    node2 = alpha_net.add_node('Node2')
    for i in range(13):
        node2.set_witness(f'steemit-acc-{i}')

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

    while True:
        pass
