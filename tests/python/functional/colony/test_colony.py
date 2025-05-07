"""Test group demonstrating basic usage of the `colony` plugin."""

from __future__ import annotations

import random

import pytest

import test_tools as tt
from hive_local_tools.functional import simultaneous_node_startup
from shared_tools.complex_networks import generate_free_addresses

WITNESSES = [f"witness-{w}" for w in range(20)]


@pytest.mark.testnet()
def test_start_colony_plugin() -> None:
    node = tt.InitNode()
    set_log_level_to_info(node)
    node.config.plugin.append("colony")
    node.run()

    assert node.is_running(), "Node didn't work"
    assert "colony" in node.config.plugin, "Colony plugin not started"


@pytest.mark.testnet()
def test_start_colony_from_block_log(
    block_log_single_sign: tt.BlockLog, alternate_chain_spec: tt.AlternateChainSpecs
) -> None:
    node = tt.InitNode()
    node.config.shared_file_size = "4G"
    set_log_level_to_info(node)
    prepare_colony_config(node)
    node.config.colony_start_at_block = block_log_single_sign.get_head_block_number()

    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    node.run(
        replay_from=block_log_single_sign,
        timeout=180,
        exit_before_synchronization=True,
        alternate_chain_specs=alternate_chain_spec,
    )

    node.run(
        time_control=tt.StartTimeControl(start_time=block_log_single_sign.get_head_block_time()),
        timeout=180,
        wait_for_live=True,
        alternate_chain_specs=alternate_chain_spec,
    )

    assert node.is_running(), "Node didn't work"
    wait_for_start_colony(node, min_trx_in_block=5000)


@pytest.mark.testnet()
def test_colony_on_basic_network_structure(
    block_log_single_sign: tt.BlockLog, alternate_chain_spec: tt.AlternateChainSpecs
) -> None:
    """
    WitnessNode ●───────────● ColonyNode
    """
    nodes = []
    witness_node = tt.WitnessNode(witnesses=["initminer", *WITNESSES])
    nodes.append(witness_node)
    witness_node.config.shared_file_size = "4G"
    for witness in WITNESSES:
        witness_node.config.private_key.append(tt.Account(witness).private_key)
    witness_node.config.private_key.append(tt.Account("initminer").private_key)
    set_log_level_to_info(witness_node)

    colony_node = tt.ApiNode()
    nodes.append(colony_node)
    colony_node.config.shared_file_size = "4G"
    set_log_level_to_info(colony_node)

    # connect nodes
    colony_node.config.p2p_endpoint = generate_free_addresses(1)[0]
    witness_node.config.p2p_seed_node = colony_node.config.p2p_endpoint

    prepare_colony_config(colony_node)
    colony_node.config.colony_start_at_block = block_log_single_sign.get_head_block_number()

    for node in nodes:
        node.run(
            replay_from=block_log_single_sign,
            timeout=180,
            alternate_chain_specs=alternate_chain_spec,
            exit_before_synchronization=True,
        )

    simultaneous_node_startup(
        [witness_node, colony_node],
        timeout=180,
        alternate_chain_specs=alternate_chain_spec,
        wait_for_live=True,
        time_control=tt.StartTimeControl(start_time=block_log_single_sign.get_head_block_time()),
    )

    assert witness_node.is_running(), "Witness node didn't work"
    assert colony_node.is_running(), "Colony node didn't work"
    wait_for_start_colony(witness_node, min_trx_in_block=5000)


@pytest.mark.testnet()
def test_colony_on_complex_network(block_log_single_sign: tt.BlockLog, alternate_chain_spec: tt.BlockLog) -> None:
    """
    ColonyNode-0 ●─────●─────● ColonyNode-1
                 │     │     │
                 ●─────●─────●
                 │     │     │
    ColonyNode-2 ●─────●─────● ColonyNode-3
    """
    nodes = []
    for witness in range(0, len(WITNESSES), 5):
        nodes.append(tt.WitnessNode(witnesses=WITNESSES[witness : witness + 5]))

    nodes[0].config.witness.append("initminer")
    nodes[0].config.private_key.append(tt.PrivateKey("initminer"))
    set_log_level_to_info(nodes[0])
    open_addresses = generate_free_addresses(4)
    for wn, address in zip(nodes, open_addresses):
        wn.config.shared_file_size = "4G"

        wn.config.p2p_endpoint = address
        wn.config.p2p_seed_node = [a for a in open_addresses if a != address]
        set_log_level_to_info(wn)
        prepare_colony_config(wn)
        wn.config.colony_start_at_block = block_log_single_sign.get_head_block_number()

    for node in nodes:
        node.run(
            replay_from=block_log_single_sign,
            timeout=180,
            alternate_chain_specs=alternate_chain_spec,
            exit_before_synchronization=True,
        )

    simultaneous_node_startup(
        nodes,
        timeout=180,
        alternate_chain_specs=alternate_chain_spec,
        wait_for_live=True,
        time_control=tt.StartTimeControl(start_time=block_log_single_sign.get_head_block_time()),
    )

    assert all(node.is_running() for node in nodes), "At least one node is not working"
    wait_for_start_colony(nodes[random.randint(0, len(nodes) - 1)], min_trx_in_block=5000)


@pytest.mark.testnet()
def test_multiple_colony_nodes_communication_with_single_witness_node(
    block_log_single_sign: tt.BlockLog, alternate_chain_spec: tt.AlternateChainSpecs
) -> None:
    """
    ColonyNode-0 ──●
                   │
    ColonyNode-1 ──●
                   │── WitnessNode
    ColonyNode-2 ──●
                   │
    ColonyNode-3 ──●
    """
    nodes: list[tt.InitNode] = []

    witness_node = tt.WitnessNode(witnesses=WITNESSES)

    witness_node.config.shared_file_size = "4G"
    witness_node.config.witness.append("initminer")
    witness_node.config.private_key.append(tt.PrivateKey("initminer"))
    set_log_level_to_info(witness_node)

    witness_node_p2p_endpoint = generate_free_addresses(1)
    witness_node.config.p2p_endpoint = witness_node_p2p_endpoint[0]

    for num in range(4):
        nodes.append(tt.ApiNode())
        nodes[num].config.shared_file_size = "4G"
        nodes[num].config.p2p_seed_node = witness_node_p2p_endpoint
        set_log_level_to_info(nodes[num])

        prepare_colony_config(nodes[num])
        nodes[num].config.colony_start_at_block = block_log_single_sign.get_head_block_number()

    nodes.append(witness_node)

    for node in nodes:
        node.run(
            replay_from=block_log_single_sign,
            timeout=180,
            alternate_chain_specs=alternate_chain_spec,
            exit_before_synchronization=True,
        )

    simultaneous_node_startup(
        nodes,
        timeout=180,
        alternate_chain_specs=alternate_chain_spec,
        wait_for_live=True,
        time_control=tt.StartTimeControl(start_time=block_log_single_sign.get_head_block_time()),
    )

    assert all(node.is_running() for node in nodes), "At least one node is not working"
    wait_for_start_colony(witness_node, min_trx_in_block=5000)


def test_colony_with_unknown_colony_signer(
    block_log_single_sign: tt.BlockLog, alternate_chain_spec: tt.AlternateChainSpecs
) -> None:
    node = tt.InitNode()
    node.config.shared_file_size = "4G"
    set_log_level_to_info(node)
    prepare_colony_config(node)
    node.config.colony_start_at_block = block_log_single_sign.get_head_block_number()

    node.config.colony_sign_with = [tt.PrivateKey("unknown-signer-account")]

    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    node.run(
        replay_from=block_log_single_sign,
        timeout=180,
        exit_before_synchronization=True,
        alternate_chain_specs=alternate_chain_spec,
    )

    node.run(
        timeout=180,
        wait_for_live=False,
        alternate_chain_specs=alternate_chain_spec,
        time_control=tt.StartTimeControl(start_time=block_log_single_sign.get_head_block_time()),
    )

    tt.Time.wait_for(
        lambda: not node.is_running(), timeout=10, timeout_error_message="Node didn't close in the allotted time"
    )
    assert not node.is_running(), "Node didn't close in the allotted time"


def prepare_colony_config(node: tt.ApiNode | tt.InitNode) -> None:
    node.config.plugin.append("colony")
    node.config.colony_sign_with = [
        "5JU5vHLxvs6RhAmUnyZHzdu6DpiJ9cbpkkqKWkD3GDpser9Qa6p"
    ]  # key for workers with single authority
    node.config.colony_article = '{"min": 100, "max": 5000, "weight": 16, "exponent": 4}'
    node.config.colony_reply = '{"min": 30, "max": 1000, "weight": 110, "exponent": 5}'
    node.config.colony_vote = '{"weight": 2070}'
    node.config.colony_transfer = '{"min": 0, "max": 350, "weight": 87, "exponent": 4}'
    node.config.colony_custom = '{"min": 20, "max": 400, "weight": 6006, "exponent": 1}'
    node.config.colony_transactions_per_block = "5000"


def wait_for_start_colony(node: tt.InitNode | tt.ApiNode, min_trx_in_block: int = 500, timeout: int = 240) -> None:
    def is_colony_started() -> bool:
        return (
            len(node.api.block.get_block(block_num=node.get_last_block_number())["block"].transactions)
            >= min_trx_in_block
        )

    tt.logger.info("Wait for start colony...")
    tt.Time.wait_for(
        is_colony_started,
        timeout=timeout,
        timeout_error_message="Colony has not reached the minimum number of transactions",
    )
    tt.logger.info("Colony started.")


def set_log_level_to_info(node: tt.InitNode | tt.ApiNode) -> None:
    node.config.log_logger = (
        '{"name":"default","level":"info","appender":"stderr"} '
        '{"name":"user","level":"info","appender":"stderr"} '
        '{"name":"chainlock","level":"info","appender":"p2p"} '
        '{"name":"sync","level":"info","appender":"p2p"} '
        '{"name":"p2p","level":"info","appender":"p2p"}'
    )
