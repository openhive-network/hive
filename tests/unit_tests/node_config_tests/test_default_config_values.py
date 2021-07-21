# This test forces that create_default_config will be always up to date.

import pytest

from test_tools import constants
from test_tools.node_configs.default import create_default_config


@pytest.fixture
def generated_config(world):
    node = world.create_raw_node()
    node.set_clean_up_policy(constants.NodeCleanUpPolicy.REMOVE_EVERYTHING)
    node.dump_config()

    return node.config


def test_default_config_values(generated_config):
    default_config = create_default_config()
    if default_config != generated_config:
        print('Found differences:')
        differences = default_config.get_differences_between(generated_config)

        for key, (default_value, generated_value) in differences.items():
            print()
            print(key)
            print(f'In default_config: {default_value} {type(default_value)}')
            print(f'Generated:         {generated_value} {type(generated_value)}')

        assert False, 'Modify config returned from create_default_config to match default generated config'
