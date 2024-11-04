from __future__ import annotations

import os
import re
import socket
import sys
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path
from typing import TYPE_CHECKING, Any, Iterable

import shared_tools.networks_architecture as networks
import test_tools as tt

from .complex_networks_helper_functions import connect_sub_networks

if TYPE_CHECKING:
    import datetime

last_used_port_number = 0


class NodesPreparer:
    def prepare(self, _: networks.NetworksBuilder) -> None:
        pass


def get_relative_time_offset_from_timestamp(timestamp: datetime.datatime) -> str:
    delta = tt.Time.now(serialize=False) - timestamp
    delta += tt.Time.seconds(5)  # Node starting and entering live mode takes some time to complete
    return f"-{delta.total_seconds():.3f}s"


def init_network(
    init_node,
    all_witness_names: list[str],
    key: str | None = None,
    block_log_directory_name: Path | None = None,
    desired_blocklog_length: int | None = None,
) -> None:
    tt.logger.info("Attaching wallets...")
    tt.logger.info(f"Witnesses: {', '.join(all_witness_names)}")
    wallet = tt.Wallet(attach_to=init_node)
    # We are waiting here for block 43, because witness participation is counting
    # by dividing total produced blocks in last 128 slots by 128. When we were waiting
    # too short, for example 42 blocks, then participation equals 42 / 128 = 32.81%.
    # It is not enough, because 33% is required. 43 blocks guarantee, that this
    # requirement is always fulfilled (43 / 128 = 33.59%, which is greater than 33%).
    tt.logger.info("Wait for block 43 (to fulfill required 33% of witness participation)")
    init_node.wait_for_block_with_number(43)

    # Prepare witnesses on blockchain
    with wallet.in_single_transaction():
        for name in all_witness_names:
            if key is None:
                wallet.api.create_account("initminer", name, "")
            else:
                wallet.api.create_account_with_keys("initminer", name, "", key, key, key, key)
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(1000))
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.update_witness(
                name,
                "https://" + name,
                tt.Account(name).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0},
            )
    tt.logger.info("Wait 21 blocks to schedule newly created witnesses into future slate")
    init_node.wait_number_of_blocks(21)

    future_witnesses = init_node.api.database.get_active_witnesses(include_future=True)["future_witnesses"]
    tt.logger.info(f"Future witnesses after voting: {future_witnesses}")

    tt.logger.info("Wait 21 blocks for future slate to become active slate")
    init_node.wait_number_of_blocks(21)

    active_witnesses = init_node.api.database.get_active_witnesses()["witnesses"]
    tt.logger.info(f"Witness state after voting: {active_witnesses}")

    # Reason of this wait is to enable moving forward of irreversible block
    tt.logger.info("Wait 21 blocks (when every witness sign at least one block)")
    init_node.wait_number_of_blocks(21)

    irreversible = init_node.api.wallet_bridge.get_dynamic_global_properties().last_irreversible_block_num
    head = init_node.api.wallet_bridge.get_dynamic_global_properties().head_block_number
    tt.logger.info(f"Network prepared, irreversible block: {irreversible}, head block: {head}")

    if desired_blocklog_length is not None:
        while irreversible < desired_blocklog_length:
            init_node.wait_number_of_blocks(1)
            irreversible = init_node.api.wallet_bridge.get_dynamic_global_properties().last_irreversible_block_num
            tt.logger.info(
                f"Generating block_log of length: {desired_blocklog_length}, "
                f"current irreversible: {irreversible}, "
                f"current head block: {init_node.api.wallet_bridge.get_dynamic_global_properties().head_block_number}"
            )

    # If a directory of `block_log` is given then it"s possible to save several files:
    # 1) block log files that contain whole saved network
    # 2) "timestamp" file that contains information about time what should be set by `libfaketime` library
    if block_log_directory_name is not None:
        # Prepare dedicated directory
        if block_log_directory_name.exists():
            block_log = tt.BlockLog(block_log_directory_name, "auto")
            for file in block_log.block_files:
                file.unlink(missing_ok=True)
        else:
            Path.mkdir(block_log_directory_name)

        # Copy newly created block log into dedicated directory
        init_node.close()
        init_node.block_log.copy_to(block_log_directory_name)


def modify_time_offset(old_iso_date: datetime, offset_in_seconds: int) -> str:
    new_iso_date = old_iso_date - tt.Time.seconds(offset_in_seconds)
    tt.logger.info(f"old date: {old_iso_date} new date(after time offset): {new_iso_date}")

    return get_relative_time_offset_from_timestamp(new_iso_date)


def run_networks(
    networks: Iterable[tt.Network], blocklog_directory: Path, time_offsets: Iterable[str] | None = None
) -> None:
    arguments = []
    alternate_chain_spec: tt.AlternateChainSpecs | None = None
    if blocklog_directory is not None:
        block_log = tt.BlockLog(blocklog_directory, "auto")
        timestamp = block_log.get_head_block_time()
        time_offset = get_relative_time_offset_from_timestamp(timestamp)
        tt.logger.info(f"'block_log' directory: {blocklog_directory} timestamp: {timestamp} time_offset: {time_offset}")
        if (acs_path := blocklog_directory / "alternate-chain-spec.json").is_file():
            alternate_chain_spec = tt.AlternateChainSpecs.parse_file(acs_path)
    else:
        tt.logger.info("'block_log' directory hasn't been defined")

    tt.logger.info("Running nodes...")

    connect_sub_networks(networks)

    nodes = [node for network in networks for node in network.nodes]

    info_nodes = ", ".join(str(node) for node in nodes)
    tt.logger.info(f"Following nodes exist in whole network: {info_nodes}")

    allow_external_time_offsets = time_offsets is not None and len(time_offsets) == len(nodes)
    tt.logger.info(f"External time offsets: {'enabled' if allow_external_time_offsets else 'disabled'}")

    assigned_addresses = generate_free_addresses(number_of_nodes=len(nodes))
    cnt_node = 1
    for iteration, node in enumerate(nodes):
        node.config.log_json_rpc = "json_rpc"
        node.config.p2p_endpoint = assigned_addresses[iteration]
        [
            node.config.p2p_seed_node.append(address)
            for address in assigned_addresses
            if address != assigned_addresses[iteration]
        ]

        if blocklog_directory is not None:
            node.config.shared_file_size = "1G"
            node.config.log_logger = (
                '{"name":"default","level":"info","appender":"stderr"}{"name":"p2p","level":"info","appender":"p2p"}'
            )
            node.run(
                replay_from=block_log,
                exit_before_synchronization=True,
                arguments=arguments,
                alternate_chain_specs=alternate_chain_spec,
            )
        cnt_node += 1

    for network in networks:
        network.is_running = True

    with ThreadPoolExecutor() as executor:
        tasks = []
        for node_num, node in enumerate(nodes):
            if blocklog_directory:
                tasks.append(
                    executor.submit(
                        partial(
                            lambda _node, _node_num: _node.run(
                                time_control=(
                                    modify_time_offset(timestamp, time_offsets[_node_num])
                                    if allow_external_time_offsets
                                    else tt.Time.serialize(timestamp, format_="@%Y-%m-%d %H:%M:%S")
                                ),
                                arguments=arguments,
                            ),
                            node,
                            node_num,
                        )
                    )
                )
            else:
                tasks.append(executor.submit(partial(lambda _node: _node.run(arguments=arguments), node)))

        for thread_number in tasks:
            thread_number.result()


def display_info(node: tt.AnyNode) -> None:
    # Network should be set up at this time, with 21 active witnesses, enough participation rate
    # and irreversible block number lagging behind around 15-20 blocks head block number
    gdpo = node.api.wallet_bridge.get_dynamic_global_properties()
    irreversible = gdpo.last_irreversible_block_num
    head = gdpo.head_block_number
    tt.logger.info(f"Network prepared, irreversible block: {irreversible}, head block: {head}")


def prepare_nodes(sub_networks_sizes: list) -> list:
    assert len(sub_networks_sizes) > 0, "At least 1 sub-network is required"

    cnt = 0
    all_witness_names = []
    sub_networks = []
    init_node = None

    for sub_networks_size in sub_networks_sizes:
        tt.logger.info(f"Preparing sub-network nr: {cnt} that consists of {sub_networks_size} witnesses")

        witness_names = [f"witness-{cnt}-{i}" for i in range(sub_networks_size)]
        all_witness_names += witness_names

        sub_network = tt.Network()
        if cnt == 0:
            init_node = tt.InitNode(network=sub_network)
        sub_networks.append(sub_network)
        tt.WitnessNode(witnesses=witness_names, network=sub_network)
        tt.ApiNode(network=sub_network)

        cnt += 1
    return sub_networks, init_node, all_witness_names


def generate_networks(
    architecture: networks.NetworksArchitecture,
    block_log_directory_name: Path | None = None,
    preparer: NodesPreparer = None,
    desired_blocklog_length: int | None = None,
) -> dict:
    builder = networks.NetworksBuilder()
    builder.build(architecture)

    if preparer is not None:
        preparer.prepare(builder)

    run_networks(builder.networks, None)

    initminer_public_key = "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"
    init_network(
        builder.init_node,
        builder.witness_names,
        initminer_public_key,
        block_log_directory_name,
        desired_blocklog_length,
    )

    return None


def launch_networks(
    architecture: networks.NetworksArchitecture,
    block_log_directory_name: Path | None = None,
    time_offsets: Iterable[int] | None = None,
    preparer: NodesPreparer = None,
) -> dict:
    builder = networks.NetworksBuilder()
    builder.build(architecture)

    if preparer is not None:
        preparer.prepare(builder)

    run_networks(builder.networks, block_log_directory_name, time_offsets)

    builder.init_wallet = tt.Wallet(attach_to=builder.init_node)

    display_info(builder.init_node)

    return builder


def generate_or_launch(
    architecture: networks.NetworksArchitecture,
    block_log_directory_name: Path | None = None,
    time_offsets: Iterable[int] | None = None,
    preparer: NodesPreparer = None,
) -> dict:
    if allow_generate_block_log():
        assert block_log_directory_name is not None, "Name of directory with block_log file must be given"
        tt.logger.info(f"New `block_log` generation: {block_log_directory_name}")
        return generate_networks(architecture, block_log_directory_name, preparer)

    return launch_networks(architecture, block_log_directory_name, time_offsets, preparer)


def allow_generate_block_log() -> bool:
    status = os.environ.get("GENERATE_NEW_BLOCK_LOG", None)
    if status is None:
        return False
    return int(status) == 1


def run_whole_network(
    architecture: networks.NetworksArchitecture,
    block_log_directory_name: Path | None = None,
    time_offsets: Iterable[int] | None = None,
    preparer: NodesPreparer = None,
) -> tuple[networks.NetworksBuilder, Any]:
    builder = generate_or_launch(architecture, block_log_directory_name, time_offsets, preparer)

    if builder is None:
        tt.logger.info("Generating 'block_log' enabled. Exiting...")
        sys.exit(1)

    return builder


def prepare_network(
    architecture: networks.NetworksArchitecture,
    block_log_directory_name: Path | None = None,
    time_offsets: Iterable[int] | None = None,
) -> networks.NetworksBuilder:
    return run_whole_network(architecture, block_log_directory_name, time_offsets)


def prepare_time_offsets(limit: int):
    time_offsets: int = []

    cnt = 0
    for _ in range(limit):
        time_offsets.append(cnt % 3 + 1)
        cnt += 1

    result = ",".join(str(time_offset) for time_offset in time_offsets)
    tt.logger.info(f"Generated: {result}")

    return time_offsets


def create_block_log_directory_name(block_log_directory_name: str):
    return Path(__file__).parent.absolute() / "block_logs" / block_log_directory_name


def generate_port_ranges(number_of_nodes: int) -> list[int]:
    global last_used_port_number

    worker_id = get_worker_id()
    start = last_used_port_number + 1 if last_used_port_number else 2000 + worker_id * 1000

    port = start
    ports = []
    while len(ports) < number_of_nodes:
        if is_port_open(port):
            ports.append(port)
        port += 1

    last_used_port_number = ports[-1]
    assert (
        last_used_port_number < 3000 + worker_id * 1000
    ), f"The pool of available ports for worker {worker_id} has been depleted."
    assert last_used_port_number <= 65535, "The maximum value of available ports has been depleted."
    return ports


def get_worker_id() -> int:
    worker = os.getenv("PYTEST_XDIST_WORKER", None)
    if worker is None or worker == "master":
        return 0
    match = re.match(r"gw(\d+)", worker)
    assert match is not None
    return int(match.group(1))


def generate_free_addresses(number_of_nodes: int) -> list[str]:
    ports = generate_port_ranges(number_of_nodes)
    return ["127.0.0.1:" + str(port) for port in ports]


def is_port_open(port: int) -> bool:
    sock = socket.socket()
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, b"\0" * 8)
    try:
        sock.bind(("127.0.0.1", port))
    except OSError:
        return False
    else:
        return True
    finally:
        sock.close()
