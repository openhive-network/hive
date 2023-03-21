import sys
from pathlib import Path
from typing import Dict

import pytest

import test_tools as tt
from shared_tools.complex_networks import create_block_log_directory_name, prepare_time_offsets, prepare_network

import shared_tools.networks_architecture as networks

@pytest.fixture
def prepare_with_many_witnesses() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[3, 3, 2, 2]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[3, 3, 2, 2]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture()
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_10_10'))

@pytest.fixture
def prepare_fork_2_sub_networks_00() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[3]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[18]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture(True)
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_fork_2_sub_networks_00'))

@pytest.fixture
def prepare_fork_2_sub_networks_01() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[6]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[17]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture(True)
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_fork_2_sub_networks_01'))

@pytest.fixture
def prepare_fork_2_sub_networks_02() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[6]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[17]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture(True)
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_fork_2_sub_networks_02'))

@pytest.fixture
def prepare_fork_2_sub_networks_03() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[3]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[18]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture(True)
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_fork_2_sub_networks_03'))

@pytest.fixture
def prepare_obi_throw_exception_00() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[10]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[11]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture(True)
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_obi_throw_exception_00'))

@pytest.fixture
def prepare_obi_throw_exception_01() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[10]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[11]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture(True)
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_obi_throw_exception_01'))

@pytest.fixture
def prepare_obi_throw_exception_02() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[10]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[11]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture(True)
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_obi_throw_exception_02'))

@pytest.fixture
def prepare_fork_3_sub_networks_00() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[3]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[4]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[14]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture(True)
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_fork_3_sub_networks_00'))

@pytest.fixture
def prepare_fork_3_sub_networks_01() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[7]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[7]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[7]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture(True)
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_fork_3_sub_networks_01'))

@pytest.fixture()
def prepare_17_3() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "ApiNode"      : True,
                            "WitnessNodes" :[17]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[3]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture()
    architecture.load(config)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_17_3'))

@pytest.fixture()
def prepare_4_4_4_4_4() -> networks.NetworksBuilder:
    config = {
        "networks": [
                        {
                            "InitNode"     : True,
                            "WitnessNodes" :[4]
                        },
                        {
                            "ApiNode"      : True,
                            "WitnessNodes" :[4]
                        },
                        {
                            "WitnessNodes" :[4]
                        },
                        {
                            "WitnessNodes" :[4]
                        },
                        {
                            "WitnessNodes" :[4]
                        }
                    ]
    }
    architecture = networks.NetworksArchitecture()
    architecture.load(config)
    time_offsets = prepare_time_offsets(architecture.nodes_number)

    yield prepare_network(architecture, create_block_log_directory_name('block_log_4_4_4_4_4'), time_offsets)