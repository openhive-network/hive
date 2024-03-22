from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from functools import partial

import pytest

import test_tools as tt
from shared_tools.complex_networks import generate_free_addresses

WITNESSES = [f"witness-{w}" for w in range(20)]


@pytest.mark.testnet()
def test_start_colony():
    # fixme: hive::chain::account_authority_object","indexed_by_typename":"hive::chain::by_account","key":"hive.fund"
    node = tt.InitNode()
    node.config.plugin.append("colony")
    node.run()


@pytest.mark.testnet()
def test_start_colony_from_block_log(block_log, alternate_chain_spec):
    node = tt.InitNode()
    set_node_parameters(node)

    # set colony config
    node.config.plugin.append("colony")

    node.config.colony_sign_with.append("5JqgqkuYAPx2xbSuttJst6Z1mTS3dJh7wdKi8b99MUcrsyT6fyM")

    node.config.colony_article = '{"min": 100, "max": 5000, "weight": 16, "exponent": 4}'
    node.config.colony_reply = '{"min": 30, "max": 1000, "weight": 110, "exponent": 5}'
    node.config.colony_vote = '{"weight": 2070}'
    node.config.colony_transfer = '{"min": 0, "max": 350, "weight": 87, "exponent": 4}'
    node.config.colony_custom = '{"min": 20, "max": 400, "weight": 6006, "exponent": 1}'

    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    # only replay
    node.run(
        replay_from=block_log,
        timeout=120,
        arguments=["--chain-id=24"],
        exit_before_synchronization=True,
        alternate_chain_specs=alternate_chain_spec,
    )

    # run
    node.run(
        replay_from=block_log,
        timeout=120,
        arguments=["--chain-id=24"],
        wait_for_live=True,
        alternate_chain_specs=alternate_chain_spec,
    )


@pytest.mark.testnet()
def test_colony_on_basic_network_structure(block_log, alternate_chain_spec):
    """
    WitnessNode ●───────────● ColonyNode
    """
    nodes = []
    witness_node = tt.WitnessNode(witnesses=["initminer", *WITNESSES])
    nodes.append(witness_node)
    set_node_parameters(witness_node)
    [witness_node.config.private_key.append(tt.Account(witness).private_key) for witness in WITNESSES]
    witness_node.config.private_key.append(tt.Account("initminer").private_key)

    colony_node = tt.ApiNode()
    nodes.append(colony_node)
    set_node_parameters(colony_node)

    # connect nodes
    colony_node.config.p2p_endpoint = generate_free_addresses(1)[0]
    witness_node.config.p2p_seed_node = colony_node.config.p2p_endpoint

    # set colony config
    colony_node.config.plugin.append("colony")

    colony_node.config.colony_start_at_block = block_log.get_head_block_number() + 10
    colony_node.config.colony_transactions_per_block = "30000"

    # only replays
    for node in nodes:
        node.run(
            replay_from=block_log,
            timeout=120,
            alternate_chain_specs=alternate_chain_spec,
            arguments=["--chain-id=24"],
            exit_before_synchronization=True,
        )

    simultaneous_node_startup(
        [witness_node, colony_node],
        timeout=120,
        alternate_chain_specs=alternate_chain_spec,
        arguments=["--chain-id=24"],
        wait_for_live=True,
        time_control=tt.StartTimeControl(start_time=block_log.get_head_block_time()),
    )


@pytest.mark.testnet()
def test_colony_on_complex_network(block_log, alternate_chain_spec) -> None:
    """
    ColonyNode-1 ●─────●─────● ColonyNode-2
                 │     │     │
                 ●─────●─────●
                 │     │     │
    ColonyNode-3 ●─────●─────● ColonyNode-4
    """
    nodes = []
    for witness in range(0, len(WITNESSES), 5):
        nodes.append(tt.WitnessNode(witnesses=WITNESSES[witness : witness + 5]))

    nodes[0].config.witness.append("initminer")
    nodes[0].config.private_key.append(tt.PrivateKey("initminer"))

    open_addresses = generate_free_addresses(4)
    for wn, address in zip(nodes, open_addresses):
        wn.config.p2p_endpoint = address
        wn.config.p2p_seed_node = [a for a in open_addresses if a != address]

        wn.config.shared_file_size = "8G"
        wn.config.log_logger = (
            '{"name":"default","level":"info","appender":"stderr"} '
            '{"name":"user","level":"error","appender":"stderr"} '
            '{"name":"chainlock","level":"error","appender":"p2p"} '
            '{"name":"sync","level":"error","appender":"p2p"} '
            '{"name":"p2p","level":"error","appender":"p2p"}'
        )
        wn.config.plugin.append("colony")

        wn.config.colony_start_at_block = block_log.get_head_block_number() + 20
        wn.config.colony_transactions_per_block = "2000"
        wn.config.colony_sign_with.append("5JqgqkuYAPx2xbSuttJst6Z1mTS3dJh7wdKi8b99MUcrsyT6fyM")
        wn.config.colony_article = '{"min": 100, "max": 5000, "weight": 16, "exponent": 4}'
        wn.config.colony_reply = '{"min": 30, "max": 1000, "weight": 110, "exponent": 5}'
        wn.config.colony_vote = '{"weight": 2070}'
        wn.config.colony_transfer = '{"min": 0, "max": 350, "weight": 87, "exponent": 4}'
        wn.config.colony_custom = '{"min": 20, "max": 400, "weight": 6006, "exponent": 1}'

    for node in nodes:
        node.run(
            replay_from=block_log,
            timeout=120,
            alternate_chain_specs=alternate_chain_spec,
            arguments=["--chain-id=24"],
            exit_before_synchronization=True,
        )

    simultaneous_node_startup(
        nodes,
        timeout=120,
        alternate_chain_specs=alternate_chain_spec,
        arguments=["--chain-id=24"],
        wait_for_live=True,
        time_control=tt.StartTimeControl(start_time=block_log.get_head_block_time()),
    )


@pytest.mark.testnet()
def test_multiple_colony_nodes_communication_with_single_witness_node(block_log, alternate_chain_spec):
    """
    ColonyNode-1 ──●
                   │
    ColonyNode-2 ──●
                   │── WitnessNode
    ColonyNode-3 ──●
                   │
    ColonyNode-4 ──●
    """
    nodes = []

    witness_node = tt.WitnessNode(witnesses=WITNESSES)

    witness_node.config.witness.append("initminer")
    witness_node.config.private_key.append(tt.PrivateKey("initminer"))
    set_node_parameters(witness_node)

    witness_node_p2p_endpoint = generate_free_addresses(1)
    witness_node.config.p2p_endpoint = witness_node_p2p_endpoint[0]

    for num in range(4):
        nodes.append(tt.ApiNode())
        set_node_parameters(nodes[num])
        nodes[num].config.p2p_seed_node = witness_node_p2p_endpoint

        nodes[num].config.plugin.append("colony")
        nodes[num].config.colony_start_at_block = block_log.get_head_block_number() + 20
        nodes[num].config.colony_transactions_per_block = "2000"
        nodes[num].config.colony_sign_with.append("5JqgqkuYAPx2xbSuttJst6Z1mTS3dJh7wdKi8b99MUcrsyT6fyM")

        nodes[num].config.colony_article = '{"min": 100, "max": 5000, "weight": 16, "exponent": 4}'
        nodes[num].config.colony_reply = '{"min": 30, "max": 1000, "weight": 110, "exponent": 5}'
        nodes[num].config.colony_vote = '{"weight": 2070}'
        nodes[num].config.colony_transfer = '{"min": 0, "max": 350, "weight": 87, "exponent": 4}'
        nodes[num].config.colony_custom = '{"min": 20, "max": 400, "weight": 6006, "exponent": 1}'

    nodes.append(witness_node)

    for node in nodes:
        node.run(
            replay_from=block_log,
            timeout=120,
            alternate_chain_specs=alternate_chain_spec,
            arguments=["--chain-id=24"],
            exit_before_synchronization=True,
        )

    simultaneous_node_startup(
        nodes,
        timeout=120,
        alternate_chain_specs=alternate_chain_spec,
        arguments=["--chain-id=24"],
        wait_for_live=True,
        time_control=tt.StartTimeControl(start_time=block_log.get_head_block_time()),
    )


def test_conf():
    node = tt.InitNode()

    node.config.colony_sign_with.append("5JqgqkuYAPx2xbSuttJst6Z1mTS3dJh7wdKi8b99MUcrsyT6fyM")

    node.config.colony_article = '{"min": 100, "max": 5000, "weight": 16, "exponent": 4}'
    node.config.colony_reply = '{"min": 30, "max": 1000, "weight": 110, "exponent": 5}'
    node.config.colony_vote = '{"weight": 2070}'
    node.config.colony_transfer = '{"min": 0, "max": 350, "weight": 87, "exponent": 4}'
    node.config.colony_custom = '{"min": 20, "max": 400, "weight": 6006, "exponent": 1}'
    node.run()


def set_node_parameters(node):
    node.config.shared_file_size = "8G"
    node.config.log_logger = (
        '{"name":"default","level":"info","appender":"stderr"} '
        '{"name":"user","level":"error","appender":"stderr"} '
        '{"name":"chainlock","level":"error","appender":"p2p"} '
        '{"name":"sync","level":"error","appender":"p2p"} '
        '{"name":"p2p","level":"debug","appender":"p2p"}'
    )


def simultaneous_node_startup(
    nodes: list,
    timeout: int,
    alternate_chain_specs: tt.AlternateChainSpecs,
    arguments: list,
    wait_for_live: bool,
    time_control: tt.StartTimeControl = None,
    exit_before_synchronization: bool = False,
):
    with ThreadPoolExecutor(max_workers=len(nodes)) as executor:
        tasks = []
        for node in nodes:
            tasks.append(
                executor.submit(
                    partial(
                        lambda _node: _node.run(
                            timeout=timeout,
                            alternate_chain_specs=alternate_chain_specs,
                            arguments=arguments,
                            wait_for_live=wait_for_live,
                            time_control=time_control,
                            exit_before_synchronization=exit_before_synchronization,
                        ),
                        node,
                    )
                )
            )
        for thread_number in tasks:
            thread_number.result()
