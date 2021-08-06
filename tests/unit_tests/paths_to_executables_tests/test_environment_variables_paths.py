def test_environment_variables_paths(paths, executables):
    for executable in executables:
        paths.set_environment_variables({executable.environment_variable: executable.path})
        assert paths.get_path_of(executable.name) == executable.path


def test_build_root_environment_variable(paths, executables):
    paths.set_environment_variables({paths.BUILD_ROOT_PATH_ENVIRONMENT_VARIABLE: '/path/to/build'})
    for executable in executables:
        assert paths.get_path_of(executable.name) == f'/path/to/build/{executable.default_relative_path}'
