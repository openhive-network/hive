from pathlib import Path

import test_tools as tt
from hive_local_tools.functional import simultaneous_node_startup
from shared_tools.complex_networks import generate_free_addresses

CONFIG = {"networks": [{"InitNode": True, "WitnessNodes": [1]}]}


def test_2_witness_nodes_escape_from_api_node():
    """
    WitnessNode-0 ( monolitic ) ●───────────●
    WitnessNode-1 ( split )     ●───────────●
    ChaseNode
    """
    block_log = tt.BlockLog(Path(__file__).parent, "monolithic")

    witness_node_0 = tt.WitnessNode(witnesses=["initminer"])
    witness_node_0.config.block_log_split = -1  # monolit

    witness_node_1 = tt.WitnessNode(witnesses=["witness-0"])
    witness_node_1.config.private_key = tt.PrivateKey("initminer")

    # connect nodes into one network
    witness_node_0.config.p2p_endpoint = generate_free_addresses(2)[0]
    witness_node_1.config.p2p_seed_node = witness_node_0.config.p2p_endpoint

    for node in [witness_node_0, witness_node_1]:
        node.run(
            replay_from=block_log,
            timeout=180,
            exit_before_synchronization=True,
        )

    witness_node_1.config.block_log_split = 1  # split

    chase_node = tt.ApiNode()
    chase_node.config.p2p_seed_node.append(witness_node_0.config.p2p_endpoint)
    chase_node.config.p2p_seed_node.append(witness_node_1.config.p2p_endpoint)

    simultaneous_node_startup(
        [witness_node_0, witness_node_1, chase_node],
        timeout=180,
        alternate_chain_specs=None,
        wait_for_live=True,
        time_control=tt.StartTimeControl(start_time=block_log.get_head_block_time()),
    )

    node.wait_number_of_blocks(10)
