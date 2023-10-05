from datetime import datetime, timezone
from typing import Dict, Optional

import pytest

import test_tools as tt

from shared_tools.complex_networks import init_network


def parse_datetime(datetime_: str) -> datetime:
    return datetime.strptime(datetime_, "%Y-%m-%dT%H:%M:%S")


def prepare_wallets(api_node):
    tt.logger.info("Attaching legacy/hf26 wallets...")

    wallet_legacy = tt.Wallet(attach_to=api_node, additional_arguments=["--transaction-serialization=legacy"])
    wallet_hf26 = tt.Wallet(attach_to=api_node, additional_arguments=["--transaction-serialization=hf26"])
    return wallet_legacy, wallet_hf26


def legacy_operation_passed(wallet):
    tt.logger.info("Creating `legacy` operations (pass expected)...")
    wallet.api.create_account("initminer", "alice", "{}")
    wallet.api.transfer("initminer", "alice", str(tt.Asset.Test(10123)), "memo")


def hf26_operation_passed(wallet):
    tt.logger.info("Creating `hf26` operations (pass expected)...")
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(199).as_nai(), "memo")


def hf26_operation_failed(wallet):
    tt.logger.info("Creating `hf26` operations (fail expected)...")

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.transfer("initminer", "alice", tt.Asset.Test(200).as_nai(), "memo")

    assert (
        "missing required active authority" in exception.value.response["error"]["message"]
    ), "Incorrect error in `hf26` transfer operation"


def run_with_faketime(node, time):
    # time example: '2020-01-01T00:00:00'
    requested_start_time = tt.Time.parse(time)
    node.run(time_offset=f'{tt.Time.serialize(requested_start_time, format_="@%Y-%m-%d %H:%M:%S")}')


def prepare_network(
    witnesses_number: int, network_name: str, allow_create_init_node: bool, allow_create_api_node: bool
):
    tt.logger.info(f"Prototypes of nodes(init, witness, api) are created...")

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
    environment_variables: Optional[Dict] = {"HIVE_HF26_TIME": f"{date_as_seconds}"}
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
    environment_variables_alpha: Optional[Dict] = {"HIVE_HF26_TIME": f"{date_as_seconds}"}

    date_as_seconds = calculate_epoch_time(hard_fork_26_time_beta)
    environment_variables_beta: Optional[Dict] = {"HIVE_HF26_TIME": f"{date_as_seconds}"}

    # June 20, 2032 9:45:38 AM
    alpha_net.run(environment_variables_alpha)

    # June 1, 2022 7:41:41 AM
    beta_net.run(environment_variables_beta)

    init_network(init_node, all_witness_names)

    return alpha_net, beta_net


def calculate_epoch_time(date):
    return int(parse_datetime(date).replace(tzinfo=timezone.utc).timestamp())
