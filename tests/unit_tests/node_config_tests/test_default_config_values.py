# This test forces that create_default_config will be always up to date.

from test_tools import World
from test_tools.node_configs.default import create_default_config


def generate_default_config():
    with World() as world:
        node = world.create_raw_node()
        node.run(use_existing_config=True, wait_for_live=False)

    from shutil import rmtree
    rmtree(node.directory)

    return node.config


def test_default_config_values():
    default_config = create_default_config()
    generated = generate_default_config()
    if default_config != generated:
        print('Found differences:')
        differences = default_config.get_differences_between(generated)

        for key, (default_value, generated_value) in differences.items():
            print()
            print(key)
            print(f'In default_config: {default_value} {type(default_value)}')
            print(f'Generated:         {generated_value} {type(generated_value)}')

        assert False, 'Modify config returned from create_default_config to match default generated config'
