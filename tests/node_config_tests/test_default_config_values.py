# This test forces that create_default_config will be always up to date.

from test_library import Node
from test_library.node_configs.default import create_default_config


def generate_default_config():
    node = Node('test_node')
    from pathlib import Path
    node.set_executable_file_path(Path(__file__).parent / '../../../../../build/programs/hived/hived')
    node.config = None
    node.run()
    node.close()

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
