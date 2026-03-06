from __future__ import annotations

import pytest

from test_tools import complex_networks as ttcn


@pytest.fixture()
def prepare_with_many_witnesses() -> ttcn.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "FullApiNode": True, "WitnessNodes": [3, 3, 2, 2]},
            {"FullApiNode": True, "WitnessNodes": [3, 3, 2, 2]},
        ]
    }
    architecture = ttcn.NetworksArchitecture()
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_10_10"))


@pytest.fixture()
def prepare_fork_2_sub_networks_00() -> ttcn.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "FullApiNode": True, "WitnessNodes": [3]},
            {"FullApiNode": True, "WitnessNodes": [18]},
        ]
    }
    architecture = ttcn.NetworksArchitecture(True)
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_fork_2_sub_networks_00"))


@pytest.fixture()
def prepare_fork_2_sub_networks_01() -> ttcn.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "FullApiNode": True, "WitnessNodes": [6]},
            {"FullApiNode": True, "WitnessNodes": [17]},
        ]
    }
    architecture = ttcn.NetworksArchitecture(True)
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_fork_2_sub_networks_01"))


@pytest.fixture()
def prepare_fork_2_sub_networks_02() -> ttcn.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "FullApiNode": True, "WitnessNodes": [6]},
            {"FullApiNode": True, "WitnessNodes": [17]},
        ]
    }
    architecture = ttcn.NetworksArchitecture(True)
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_fork_2_sub_networks_02"))


@pytest.fixture()
def prepare_fork_2_sub_networks_03() -> ttcn.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "FullApiNode": True, "WitnessNodes": [3]},
            {"FullApiNode": True, "WitnessNodes": [18]},
        ]
    }
    architecture = ttcn.NetworksArchitecture(True)
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_fork_2_sub_networks_03"))


@pytest.fixture()
def prepare_obi_throw_exception_00() -> ttcn.NetworksBuilder:
    config = {
        "networks": [{"InitNode": True, "ApiNode": True, "WitnessNodes": [10]}, {"ApiNode": True, "WitnessNodes": [11]}]
    }
    architecture = ttcn.NetworksArchitecture(True)
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_obi_throw_exception_00"))


@pytest.fixture()
def prepare_obi_throw_exception_01() -> ttcn.NetworksBuilder:
    config = {
        "networks": [{"InitNode": True, "ApiNode": True, "WitnessNodes": [10]}, {"ApiNode": True, "WitnessNodes": [11]}]
    }
    architecture = ttcn.NetworksArchitecture(True)
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_obi_throw_exception_01"))


@pytest.fixture()
def prepare_obi_throw_exception_02() -> ttcn.NetworksBuilder:
    config = {
        "networks": [{"InitNode": True, "ApiNode": True, "WitnessNodes": [10]}, {"ApiNode": True, "WitnessNodes": [11]}]
    }
    architecture = ttcn.NetworksArchitecture(True)
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_obi_throw_exception_02"))


@pytest.fixture()
def prepare_fork_3_sub_networks_00() -> ttcn.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "ApiNode": True, "WitnessNodes": [3]},
            {"ApiNode": True, "WitnessNodes": [4]},
            {"ApiNode": True, "WitnessNodes": [14]},
        ]
    }
    architecture = ttcn.NetworksArchitecture(True)
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_fork_3_sub_networks_00"))


@pytest.fixture()
def prepare_fork_3_sub_networks_01() -> ttcn.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "ApiNode": True, "WitnessNodes": [7]},
            {"ApiNode": True, "WitnessNodes": [7]},
            {"ApiNode": True, "WitnessNodes": [7]},
        ]
    }
    architecture = ttcn.NetworksArchitecture(True)
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_fork_3_sub_networks_01"))


@pytest.fixture()
def prepare_17_3() -> ttcn.NetworksBuilder:
    config = {
        "networks": [{"InitNode": True, "ApiNode": True, "WitnessNodes": [17]}, {"ApiNode": True, "WitnessNodes": [3]}]
    }
    architecture = ttcn.NetworksArchitecture()
    architecture.load(config)
    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_17_3"))


@pytest.fixture()
def prepare_4_4_4_4_4() -> ttcn.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "WitnessNodes": [4]},
            {"ApiNode": True, "WitnessNodes": [4]},
            {"WitnessNodes": [4]},
            {"WitnessNodes": [4]},
            {"WitnessNodes": [4]},
        ]
    }
    architecture = ttcn.NetworksArchitecture()
    architecture.load(config)
    time_offsets = ttcn.prepare_time_offsets(architecture.nodes_number)

    return ttcn.prepare_network(architecture, ttcn.create_block_log_directory_name("block_log_4_4_4_4_4"), time_offsets)
