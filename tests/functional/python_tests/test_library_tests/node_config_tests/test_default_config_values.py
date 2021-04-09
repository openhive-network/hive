# This test forces that DEFAULT_CONFIG will be always up to date.

from test_library import Node
from test_library.node_configs.default import DEFAULT_CONFIG


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
    generated = generate_default_config()
    if DEFAULT_CONFIG != generated:
        print('Found differences:')
        differences = DEFAULT_CONFIG.get_differences_between(generated)

        for key, (default_value, generated_value) in differences.items():
            print()
            print(key)
            print('In DEFAULT_CONFIG:', default_value)
            print('Generated:        ', generated_value)

        assert False, 'Modify DEFAULT_CONFIG to match default generated config'
