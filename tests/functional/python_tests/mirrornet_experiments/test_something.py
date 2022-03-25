from test_tools import BlockLog, logger


def test_something(world):
    chain_id = '42'
    key_to_all_accounts_in_mirrornet = 'key-to-all-accounts-in-mirrornet'

    first_witness_node = world.create_witness_node(witnesses=['witness0', 'witness1'])
    first_witness_node.config.private_key = key_to_all_accounts_in_mirrornet
    first_witness_node.run(
        replay_from=BlockLog(None, '/path/to/block_log'),
        arguments=[f'--chain-id={chain_id}'],
    )

    # Here first witness node will have completed replay and will be live (will generate blocks)

    second_witness_node = world.create_witness_node(witnesses=['witness2', 'witness3'])
    second_witness_node.config.p2p_seed_node = first_witness_node.get_p2p_endpoint()
    second_witness_node.config.private_key = key_to_all_accounts_in_mirrornet
    second_witness_node.run(arguments=[f'--chain-id={chain_id}'])

    # Here second witness node will be live

    # Example call
    response = second_witness_node.api.database.list_accounts(start='', limit=100, order='by_name')
    logger.info([account['name'] for account in response['accounts']])
