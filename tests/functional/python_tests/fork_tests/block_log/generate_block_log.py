from pathlib import Path

import test_tools as tt


def prepare_blocklog():
    alpha_witness_names = [f'witness{i}-alpha' for i in range(10)]
    beta_witness_names = [f'witness{i}-beta' for i in range(10)]
    all_witness_names = alpha_witness_names + beta_witness_names

    # Create first network
    alpha_net = tt.Network()  # TODO: Add network name prefix, e.g. AlphaNetwork0 (Alpha- is custom prefix)
    init_node = tt.InitNode(network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(0, 3)], network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(3, 6)], network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(6, 8)], network=alpha_net)
    tt.WitnessNode(witnesses=[f'witness{i}-alpha' for i in range(8, 10)], network=alpha_net)
    tt.ApiNode(network=alpha_net)

    # Create second network
    beta_net = tt.Network()  # TODO: Add network name prefix, e.g. AlphaNetwork0 (Alpha- is custom prefix)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(0, 3)], network=beta_net)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(3, 6)], network=beta_net)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(6, 8)], network=beta_net)
    tt.WitnessNode(witnesses=[f'witness{i}-beta' for i in range(8, 10)], network=beta_net)
    tt.ApiNode(network=beta_net)

    # Run
    alpha_net.connect_with(beta_net)

    tt.logger.info('Running networks, waiting for live...')
    alpha_net.run()
    beta_net.run()

    tt.logger.info('Attaching wallets...')
    wallet = tt.Wallet(attach_to=init_node)
    # We are waiting here for block 43, because witness participation is counting
    # by dividing total produced blocks in last 128 slots by 128. When we were waiting
    # too short, for example 42 blocks, then participation equals 42 / 128 = 32.81%.
    # It is not enough, because 33% is required. 43 blocks guarantee, that this
    # requirement is always fulfilled (43 / 128 = 33.59%, which is greater than 33%).
    tt.logger.info('Wait for block 43 (to fulfill required 33% of witness participation)')
    init_node.wait_for_block_with_number(43)

    # Prepare witnesses on blockchain
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.create_account('initminer', name, '')
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(1000))
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.update_witness(
                name, "https://" + name,
                tt.Account(name).keys.public,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

    tt.logger.info('Wait 21 blocks to schedule newly created witnesses into future slate')
    init_node.wait_number_of_blocks(21)

    future_witnesses = init_node.api.database.get_active_witnesses(include_future=True)["future_witnesses"]
    tt.logger.info(f"Future witnesses after voting: {future_witnesses}")
    assert len(future_witnesses) == 21

    tt.logger.info('Wait 21 blocks for future slate to become active slate')
    init_node.wait_number_of_blocks(21)

    active_witnesses = init_node.api.database.get_active_witnesses()["witnesses"]
    tt.logger.info(f"Witness state after voting: {active_witnesses}")
    assert len(active_witnesses) == 21

    # Reason of this wait is to enable moving forward of irreversible block
    tt.logger.info('Wait 21 blocks (when every witness sign at least one block)')
    init_node.wait_number_of_blocks(21)

    result = wallet.api.info()
    head_block_num = result['head_block_number']
    timestamp = init_node.api.block.get_block(block_num=head_block_num)['block']['timestamp']
    tt.logger.info(f'head block timestamp: {timestamp}')

    Path('block_log').unlink(missing_ok=True)

    init_node.close()
    init_node.block_log.copy_to(Path(__file__).parent.absolute())

    with open('timestamp', 'w') as f:
        f.write(f'{timestamp}')


if __name__ == "__main__":
    prepare_blocklog()
