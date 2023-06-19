import pytest

import test_tools as tt
from shared_tools.complex_networks import create_block_log_directory_name, prepare_time_offsets, prepare_network
import shared_tools.networks_architecture as networks


@pytest.fixture()
def prepare_4_4_4_4_4_with_time_offset() -> networks.NetworksBuilder:
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
    time_offsets_list = (architecture.nodes_number-1) * [4] + [0]
    time_offsets_string = ','.join(str(time_offset) for time_offset in time_offsets_list)
    yield prepare_network(architecture, create_block_log_directory_name('block_log_4_4_4_4_4'), time_offsets_string)
