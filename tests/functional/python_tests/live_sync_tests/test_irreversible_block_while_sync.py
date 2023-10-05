import test_tools as tt


def test_irreversible_block_while_sync():
    net = tt.Network()
    init_node = tt.InitNode(network=net)
    api_node = tt.ApiNode(network=net)

    tt.logger.info("Running network, waiting for live sync...")
    net.run()

    # PREREQUISITES
    # We need to be before head block number HIVE_START_MINER_VOTING_BLOCK constant (30 in testnet). Also head block
    # number must be greater than HIVE_MAX_WITNESSES (21 in both testnet and mainnet).
    tt.logger.info("Waiting for block number 23...")
    init_node.wait_for_block_with_number(23)

    # TRIGGER
    # Node restart at block n, HIVE_MAX_WITNESSES < n < HIVE_START_MINER_VOTING_BLOCK.
    tt.logger.info("Restarting api node...")
    api_node.restart(timeout=90)

    # VERIFY
    # No crash.
