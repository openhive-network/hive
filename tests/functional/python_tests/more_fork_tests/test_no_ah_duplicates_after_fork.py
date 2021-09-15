from more_fork_tests.local_tools import assert_no_duplicates, generate, ProducingBlocksInBackgroundContext
import time
from test_tools import logger, Account, World, Asset

FIRST_BLOCK_TIMESTAMP = int(time.time()) - 300 * 3


def test_no_ah_duplicates_after_fork():
    with World() as world:
        alpha_witness_names = [f'witness{i}-alpha' for i in range(11)]
        beta_witness_names = [f'witness{i}-beta' for i in range(10)]
        all_witness_names = alpha_witness_names + beta_witness_names

        # Create first network
        alpha_net = world.create_network('Alpha')
        debug_node = alpha_net.create_api_node(name='DebugNode')
        alpha_node = alpha_net.create_witness_node(witnesses=[])
        api_node = alpha_net.create_api_node()

        # Create second network
        beta_net = world.create_network('Beta')
        beta_node = beta_net.create_witness_node(witnesses=[])
        beta_net.create_api_node()

        # Run
        alpha_net.connect_with(beta_net)

        logger.info('Running networks, NOT waiting for live...')
        alpha_net.run(wait_for_live=False)
        beta_net.run(wait_for_live=False)

        timestamp = FIRST_BLOCK_TIMESTAMP
        debug_node.timestamp = timestamp

        for i in range(128):
            generate(debug_node, 0.01)

        logger.info('Attaching wallets...')
        wallet = debug_node.attach_wallet()

        with ProducingBlocksInBackgroundContext(debug_node, 1):
            with wallet.in_single_transaction():
                for name in all_witness_names:
                    wallet.api.create_account('initminer', name, '')
                logger.info('create_account created...')
        logger.info('create_account sent...')


        with ProducingBlocksInBackgroundContext(debug_node, 1):
            with wallet.in_single_transaction():
                for name in all_witness_names:
                    wallet.api.transfer_to_vesting("initminer", name, Asset.Test(1000))
                logger.info('transfer_to_vesting created...')
        logger.info('transfer_to_vesting sent...')


        with ProducingBlocksInBackgroundContext(debug_node, 1):
            with wallet.in_single_transaction():
                for name in all_witness_names:
                    wallet.api.update_witness(
                        name, "https://" + name,
                        Account(name).public_key,
                        {"account_creation_fee": Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
                    )
                logger.info('update_witness created...')
        logger.info('update_witness sent...')



        while timestamp < time.time() + 20:
            generate(debug_node, 0.01)
            timestamp += 3


        logger.info("Witness state after voting")
        response = api_node.api.database.list_witnesses(start=0, limit=100, order='by_name')
        active_witnesses = response["result"]["witnesses"]
        active_witnesses_names = [witness["owner"] for witness in active_witnesses]
        logger.info(active_witnesses_names)
        assert len(active_witnesses_names) == 22


        alpha_node.close()
        for witness in [f'witness{i}-alpha' for i in range(0, 11)]:
            World._NodesCreator__register_witness(alpha_node, witness)
        #alpha_node.config.enable_stale_production = True
        #alpha_node.config.required_participation = 0
        alpha_node.run(wait_for_live=True, timeout = 6000)

        beta_node.close()
        for witness in [f'witness{i}-beta' for i in range(0, 10)]:
            World._NodesCreator__register_witness(beta_node, witness)
        beta_node.run(wait_for_live=True, timeout = 6000)


        # TRIGGER
        # Using newtork_node_api we block communication between alpha and beta parts.
        # After some time (when irreversible block number increases in both subnetworks) we join alpha and beta part again
        trigger_fork(alpha_net, beta_net)

        # VERIFY
        # Expected behaviour is that nodes in one of subnetworks (random one, alpha or beta) will perform undo and enter live sync
        # We check there are no duplicates in account_history_api after such scenario (issue #117).
        logger.info("Assert there are no duplicates in account_history.get_ops_in_block after fork...")
        assert_no_duplicates(alpha_node, beta_node)
        # while True:
        #     debug_node.wait_number_of_blocks(1)


def trigger_fork(alpha_net, beta_net):
    alpha_node = alpha_net.node('ApiNode0')
    beta_node = beta_net.node('ApiNode0')

    alpha_net.disconnect_from(beta_net)
    logger.info(f'Disconnected {alpha_net} and {beta_net}')

    # wait until irreversible block number increases in both subnetworks
    irreversible_block_number_during_disconnection = alpha_node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
    logger.info(f'irreversible_block_number_during_disconnection: {irreversible_block_number_during_disconnection}')
    logger.info('Waiting until irreversible block number increases in both subnetworks')
    for node in [alpha_node, beta_node]:
        while True:
            current_irreversible_block = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
            logger.info(f'Irreversible in {node}: {current_irreversible_block}')
            if current_irreversible_block > irreversible_block_number_during_disconnection:
                break
            node.wait_number_of_blocks(1)

    alpha_net.connect_with(beta_net)
    logger.info(f'Reconnected {alpha_net} and {beta_net}')
