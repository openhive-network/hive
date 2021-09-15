from test_tools import logger, Account, World, Asset
from more_fork_tests.local_tools import generate, ProducingBlocksInBackgroundContext
import time
import threading
import math


FIRST_BLOCK_TIMESTAMP = int(time.time()) - 200 * 3
BLOCKS_IN_FORK = 10
SKIP_WITNESS_SIGNATURE = 1

FORKS = 5

def test_trigger_undo():
    with World() as world:
        alpha_witness_names = [f'witness{i}-alpha' for i in range(21)]

        # Create first network
        alpha_net = world.create_network('Alpha')
        debug_node = alpha_net.create_api_node(name='DebugNode')
        witness_node = alpha_net.create_witness_node(witnesses=alpha_witness_names)
        witness_node.config.enable_stale_production = True
        witness_node.config.required_participation = 0

        # Create second network
        beta_net = world.create_network('Beta')
        api_node = beta_net.create_api_node()
        api_node2 = beta_net.create_api_node()

        api_node2.config.plugin.append('sql_serializer')
        api_node2.config.psql_url = 'postgresql://myuser:mypassword@localhost/haf_block_log'

        # Run
        alpha_net.connect_with(beta_net)

        logger.info('Running networks, NOT waiting for live...')
        alpha_net.run(wait_for_live=False)
        beta_net.run(wait_for_live=False)

        timestamp = FIRST_BLOCK_TIMESTAMP
        debug_node.timestamp = timestamp

        for i in range(128):
            generate(debug_node, 0.03)

        logger.info('Attaching wallet to debug node...')
        wallet = debug_node.attach_wallet(timeout=60)

        with ProducingBlocksInBackgroundContext(debug_node, 1):
            with wallet.in_single_transaction():
                for name in alpha_witness_names:
                    wallet.api.create_account('initminer', name, '')
                logger.info('create_account created...')
        logger.info('create_account sent...')


        with ProducingBlocksInBackgroundContext(debug_node, 1):
            with wallet.in_single_transaction():
                for name in alpha_witness_names:
                    wallet.api.transfer_to_vesting("initminer", name, Asset.Test(1000))
                logger.info('transfer_to_vesting created...')
        logger.info('transfer_to_vesting sent...')


        with ProducingBlocksInBackgroundContext(debug_node, 1):
            with wallet.in_single_transaction():
                for name in alpha_witness_names:
                    wallet.api.update_witness(
                        name, "https://" + name,
                        Account(name).public_key,
                        {"account_creation_fee": Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
                    )
                logger.info('update_witness created...')
        logger.info('update_witness sent...')


        witness_node.api.debug_node.debug_switch_witness_production( next = True )


        while debug_node.timestamp < time.time():
            generate(debug_node, 0.03)

        head = debug_node.get_last_block_number()
        for node in [witness_node, api_node]:
            node.wait_for_block_with_number(head)

        logger.info("Witness state after voting")
        response = api_node.api.database.list_witnesses(start=0, limit=100, order='by_name')
        active_witnesses = response["result"]["witnesses"]
        active_witnesses_names = [witness["owner"] for witness in active_witnesses]
        logger.info(active_witnesses_names)
        assert len(active_witnesses_names) == 22




        witness_node.api.debug_node.debug_switch_witness_production( next = False )



        api_node.timestamp = None
        wallet = api_node.attach_wallet()

        for fork_number in range(FORKS):
            logger.info(f"fork number {fork_number}")
            fork_block = api_node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]
            head_block = fork_block
            alpha_net.disconnect_from(beta_net)

            generate(api_node, 0.3)
            with ProducingBlocksInBackgroundContext(api_node, 1):
                with wallet.in_single_transaction():
                    name = "fork-" + str(fork_number)
                    wallet.api.create_account('initminer', name, '')
                    logger.info('create_account created...')
            logger.info('create_account sent...')
            while head_block < fork_block + BLOCKS_IN_FORK:
                generate(api_node, 0.3)
                head_block = api_node.api.database.get_dynamic_global_properties()["result"]["head_block_number"]

            witness_node.wait_for_block_with_number(head_block + 1)
            alpha_net.connect_with(beta_net)
            api_node.wait_for_block_with_number(head_block + 2)
