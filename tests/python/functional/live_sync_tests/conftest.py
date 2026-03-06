from __future__ import annotations

import pytest

from test_tools import complex_networks as ttcn


@pytest.fixture()
def prepare_4_4_4_4_4_with_time_offset_and_full_api_node() -> ttcn.NetworksBuilder:
    config = {
        "networks": [
            {"InitNode": True, "WitnessNodes": [4]},
            {"FullApiNode": True, "WitnessNodes": [4]},
            {"WitnessNodes": [4]},
            {"WitnessNodes": [4]},
            {"WitnessNodes": [4]},
        ]
    }
    architecture = ttcn.NetworksArchitecture()
    architecture.load(config)
    time_offsets_list = [0, 0, 0, 0, 0, 0, -4]
    return ttcn.prepare_network(
        architecture,
        ttcn.create_block_log_directory_name("block_log_4_4_4_4_4_with_full_api_node"),
        time_offsets_list,
    )
