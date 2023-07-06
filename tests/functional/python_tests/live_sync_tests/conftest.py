import pytest

from shared_tools.complex_networks import (
    create_block_log_directory_name,
    prepare_network,
)
import shared_tools.networks_architecture as networks


@pytest.fixture()
def prepare_4_4_4_4_4_with_time_offset_and_full_api_node() -> networks.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "WitnessNodes": [4]},
            {"FullApiNode": True, "WitnessNodes": [4]},
            {"WitnessNodes": [4]},
            {"WitnessNodes": [4]},
            {"WitnessNodes": [4]},
        ]
    }
    architecture = networks.NetworksArchitecture()
    architecture.load(config)
    time_offsets_list = [0, 0, 0, 0, 0, 0, -4]
    yield prepare_network(
        architecture,
        create_block_log_directory_name("block_log_4_4_4_4_4_with_full_api_node"),
        time_offsets_list,
    )
