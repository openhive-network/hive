from __future__ import annotations

import pytest

import test_tools as tt
from shared_tools.complex_networks import init_network


def run_with_faketime(node, time):
    # time example: '2020-01-01T00:00:00'
    requested_start_time = tt.Time.parse(time)
    node.run(time_control=f'{tt.Time.serialize(requested_start_time, format_="@%Y-%m-%d %H:%M:%S")}')


@pytest.fixture()
def node_hf25() -> tt.InitNode:
    node = tt.InitNode()
    run_with_faketime(node, "2022-10-10T10:10:10")
    return node


@pytest.fixture()
def node_hf26() -> tt.InitNode:
    node = tt.InitNode()
    node.run(
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(tt.Time.now(serialize=False).timestamp()),
            hardfork_schedule=[
                tt.HardforkSchedule(hardfork=26, block_num=1),
                tt.HardforkSchedule(hardfork=26 + 1, block_num=2000),
            ],
            hive_owner_update_limit=60,
        )
    )

    return node


@pytest.fixture()
def wallet_hf25(node_hf25) -> tt.Wallet:
    return tt.Wallet(attach_to=node_hf25)


@pytest.fixture()
def wallet_hf26(node_hf26) -> tt.Wallet:
    return tt.Wallet(attach_to=node_hf26)


def prepare_network(
    witnesses_number: int, network_name: str, allow_create_init_node: bool, allow_create_api_node: bool
):
    tt.logger.info("Prototypes of nodes(init, witness, api) are created...")

    witness_names = [f"wit{i}-{network_name}" for i in range(witnesses_number)]

    network = tt.Network()

    init_node = None
    if allow_create_init_node:
        init_node = tt.InitNode(network=network)

    tt.WitnessNode(witnesses=witness_names, network=network)

    api_node = None
    if allow_create_api_node:
        api_node = tt.ApiNode(network=network)

    return witness_names, network, init_node, api_node


def prepare_environment(hard_fork_26_time):
    all_witness_names, alpha_net, init_node, api_node = prepare_network(
        witnesses_number=20, network_name="alpha", allow_create_init_node=True, allow_create_api_node=True
    )

    # Run
    tt.logger.info("Running networks, waiting for live...")
    date_as_seconds = calculate_epoch_time(hard_fork_26_time)
    environment_variables: dict | None = {"HIVE_HF26_TIME": f"{date_as_seconds}"}
    alpha_net.run(environment_variables)

    init_network(init_node, all_witness_names)

    return alpha_net


def prepare_environment_with_2_sub_networks(hard_fork_26_time_alpha, hard_fork_26_time_beta):
    # Because HIVE_HARDFORK_REQUIRED_WITNESSES = 17 // 17 of the 21 dpos witnesses (20 elected and 1 chosen) required for hardfork
    witness_names_alpha, alpha_net, init_node, api_node = prepare_network(
        witnesses_number=8, network_name="alpha", allow_create_init_node=True, allow_create_api_node=True
    )
    witness_names_beta, beta_net, init_node2, api_node2 = prepare_network(
        witnesses_number=12, network_name="beta", allow_create_init_node=False, allow_create_api_node=False
    )

    all_witness_names = witness_names_alpha + witness_names_beta

    alpha_net.connect_with(beta_net)

    # Run
    tt.logger.info("Running networks, waiting for live...")

    date_as_seconds = calculate_epoch_time(hard_fork_26_time_alpha)
    environment_variables_alpha: dict | None = {"HIVE_HF26_TIME": f"{date_as_seconds}"}

    date_as_seconds = calculate_epoch_time(hard_fork_26_time_beta)
    environment_variables_beta: dict | None = {"HIVE_HF26_TIME": f"{date_as_seconds}"}

    # June 20, 2032 9:45:38 AM
    alpha_net.run(environment_variables_alpha)

    # June 1, 2022 7:41:41 AM
    beta_net.run(environment_variables_beta)

    init_network(init_node, all_witness_names)

    return alpha_net, beta_net


def calculate_epoch_time(date):
    return int(tt.Time.parse(date).timestamp())


@pytest.fixture()
def network_before_hf26():
    tt.logger.info("Preparing fixture network_before_hf26")

    alpha_net = prepare_environment("2023-11-08T07:41:40")

    return {"alpha": alpha_net}


@pytest.fixture()
def network_after_hf26():
    tt.logger.info("Preparing fixture network_after_hf26")

    alpha_net = prepare_environment("2022-06-01T07:41:41")

    return {"alpha": alpha_net}


@pytest.fixture()
def network_after_hf26_without_majority():
    tt.logger.info("Preparing fixture network_after_hf26_without_majority")

    alpha_net, beta_net = prepare_environment_with_2_sub_networks("2032-06-20T09:45:38", "2022-06-01T07:41:41")

    return {
        "alpha": alpha_net,
        "beta": beta_net,
    }
