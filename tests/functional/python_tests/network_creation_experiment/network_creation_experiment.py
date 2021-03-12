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

    alpha_net.add_node('Node0')
    alpha_net.add_node('Node1')
    alpha_net.add_node('Node2')

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
