from fixtures import paths


def test_command_line_arguments_paths(paths):
    paths.parse_command_line_arguments(['--hived-path', 'example/hive/path'])
    assert paths.get_path_of('hived') == 'example/hive/path'
