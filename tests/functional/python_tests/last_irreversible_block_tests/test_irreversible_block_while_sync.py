from test_tools import Account, logger, World


def test_irreversible_block_while_sync():
    with World() as world:
        alpha_witness_names = [f'witness{i}-alpha' for i in range(11)]
        beta_witness_names = [f'witness{i}-beta' for i in range(10)]
        all_witness_names = alpha_witness_names + beta_witness_names

        # Create first network
        alpha_net = world.create_network('Alpha')
        init_node = alpha_net.create_init_node()
        alpha_node = alpha_net.create_node()

        # Create second network
        beta_net = world.create_network('Beta')
        beta_node = beta_net.create_node()
        api_node = beta_net.create_node()

        # Create witnesses
        for name in alpha_witness_names:
            alpha_node.config.witness.append(name)

        for name in beta_witness_names:
            beta_node.config.witness.append(name)

        # Prepare config
        for node in alpha_net.nodes + beta_net.nodes:
            node.config.shared_file_size = '54M'
            node.config.plugin.extend([
                'network_broadcast_api', 'network_node_api', 'account_history_rocksdb', 'account_history_api'
            ])

        # Run
        alpha_net.connect_with(beta_net)

        logger.info('Running networks, waiting for live...')
        alpha_net.run()
        beta_net.run()

        logger.info('Attaching wallets...')
        wallet = init_node.attach_wallet()
        beta_wallet = beta_node.attach_wallet()

        # We are waiting here for block 43, because witness participation is counting
        # by dividing total produced blocks in last 128 slots by 128. When we were waiting
        # too short, for example 42 blocks, then participation equals 42 / 128 = 32.81%.
        # It is not enough, because 33% is required. 43 blocks guarantee, that this
        # requirement is always fulfilled (43 / 128 = 33.59%, which is greater than 33%).
        logger.info('Wait for block 43 (to fulfill required 33% of witness participation)')
        init_node.wait_for_block_with_number(43)

        # Prepare witnesses on blockchain
        with wallet.in_single_transaction():
            for name in all_witness_names:
                wallet.api.create_account('initminer', name, '')
        with wallet.in_single_transaction():
            for name in all_witness_names:
                wallet.api.transfer_to_vesting("initminer", name, "1000.000 TESTS")
        with wallet.in_single_transaction():
            for name in all_witness_names:
                wallet.api.update_witness(
                    name, "https://" + name,
                    Account(name).public_key,
                    {"account_creation_fee": "3.000 TESTS", "maximum_block_size": 65536, "sbd_interest_rate": 0}
                )


        logger.info('Wait 21 blocks to schedule newly created witnesses')
        init_node.wait_number_of_blocks(21)

        logger.info("Witness state after voting")
        response = node.api.database.list_witnesses(start=0, limit=100, order='by_name')
        active_witnesses = response["result"]["witnesses"]
        active_witnesses_names = [witness["owner"] for witness in active_witnesses]
        logger.info(active_witnesses_names)
        assert len(active_witnesses_names) == 22

        # Reason of this wait is to enable moving forward of irreversible block
        logger.info('Wait 21 blocks (when every witness sign at least one block)')
        init_node.wait_number_of_blocks(21)

        # Network should be set up at this time, with 21 active witnesses, enough participation rate
        # and irreversible block number lagging behind around 15-20 blocks head block number
        result = wallet.api.info()["result"]
        irreversible = result["last_irreversible_block_num"]
        head = result["head_block_num"]
        logger.info(f'Network prepared, irreversible block: {irreversible}, head block: {head}')

        assert irreversible + 10 < head

        # Test no duplicates after fork
        alpha_net.disconnect_from(beta_net)
        logger.info('Disconnected alpha_net from beta_net')

        irreversible_block_number_during_disconnection = wallet.api.info()["result"]["last_irreversible_block_num"]
        logger.info(f'irreversible_block_number_during_disconnection: {irreversible_block_number_during_disconnection}')
        logger.info('Waiting until irreversible block number increase in both subnetworks')
        for node in [alpha_node, beta_node]:
            while True:
                current_irreversible_block = node.api.database.get_dynamic_global_properties()["result"]["last_irreversible_block_num"]
                logger.info(f'Irreversible in {node}: {current_irreversible_block}')
                if current_irreversible_block > irreversible_block_number_during_disconnection:
                    break
                node.wait_number_of_blocks(1)

        alpha_net.connect_with(beta_net)
        logger.info('Reconnected')

        logger.info("Checking there are no duplicates in account_history.get_ops_in_block after fork...")
        for _ in range(10):
            alpha_irreversible = wallet.api.info()["result"]["last_irreversible_block_num"]
            alpha_reward_operations = count_ops_by_type(alpha_node, 'producer_reward_operation', alpha_irreversible, limit=50)
            beta_irreversible = beta_wallet.api.info()["result"]["last_irreversible_block_num"]
            beta_reward_operations = count_ops_by_type(beta_node, 'producer_reward_operation', beta_irreversible, limit=50)

            assert sum(i==1 for i in alpha_reward_operations.values()) == 50
            assert sum(i==1 for i in beta_reward_operations.values()) == 50

            alpha_node.wait_number_of_blocks(1)
        logger.info("No there are no duplicates in account_history.get_ops_in_block after fork...")

        # Test no duplicates after node restart
        logger.info("Restarting api node...")
        api_node.close()
        api_node.run()

        logger.info("Checking there are no duplicates in account_history.get_ops_in_block after node restart...")
        for _ in range(10):
            beta_irreversible = beta_wallet.api.info()["result"]["last_irreversible_block_num"]
            api_node_reward_operations = count_ops_by_type(api_node, 'producer_reward_operation', beta_irreversible, limit=50)

            assert sum(i==1 for i in api_node_reward_operations.values()) == 50

            api_node.wait_number_of_blocks(1)
        logger.info("No there are no duplicates in account_history.get_ops_in_block after node restart...")

        # Test enum_virtual_ops for head block returns only virtual ops
        account_name = 'gamma-1'
        wallet.api.create_account('initminer', account_name, '')
        init_node.wait_number_of_blocks(1)

        logger.info("Checking there are only virtual operations in account_history.enum_virtual_ops...")
        for _ in range(10):
            wallet.api.transfer_to_vesting('initminer', account_name, "1.000 TESTS")

            head_block = wallet.api.info()["result"]["head_block_number"]
            block_to_check = head_block - 1

            response = api_node.api.account_history.enum_virtual_ops(block_range_begin=block_to_check, block_range_end=block_to_check+1, include_reversible=True)
            ops = response["result"]["ops"]
            logger.info(str([op["op"]["type"] for op in ops]) + f" in block  {block_to_check}")

            for op in ops:
                virtual_op = op["virtual_op"]
                assert virtual_op>0
