def test_command_line_arguments_paths(paths, executables):
    for executable in executables:
        paths.parse_command_line_arguments([executable.argument, executable.path])
        assert paths.get_path_of(executable.name) == executable.path


def test_build_root_command_line_argument(paths, executables):
    paths.parse_command_line_arguments([paths.BUILD_ROOT_PATH_COMMAND_LINE_ARGUMENT, '/path/to/build'])
    for executable in executables:
        assert paths.get_path_of(executable.name) == f'/path/to/build/{executable.default_relative_path}'
