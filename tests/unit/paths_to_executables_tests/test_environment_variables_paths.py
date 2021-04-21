from fixtures import paths, executables


def test_environment_variables_paths(paths, executables):
    for executable in executables:
        paths.set_environment_variables({executable.environment_variable: executable.path})
        assert paths.get_path_of(executable.name) == executable.path
