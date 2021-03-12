from network import Network


if __name__ == "__main__":
    net = Network('TestNetwork')
    net.set_hived_executable_file_path('../../../../build/programs/hived/hived')
    net.set_wallet_executable_file_path('../../../../build/programs/cli_wallet/cli_wallet')

    init_node = net.add_node('InitNode')
    init_node.set_witness('initminer', '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n')

    net.add_node('TestNode0')
    net.add_node('TestNode1')
    net.add_node('TestNode2')

    net.run()

    wallet = net.attach_wallet()

    while True:
        pass
