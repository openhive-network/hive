def test_manually_set_path_priority(paths, executable):
    paths.set_path_of(executable.name, 'path_set_manually')
    paths.parse_command_line_arguments([executable.argument, 'path_set_from_command_line_arguments'])
    paths.set_environment_variables({executable.environment_variable: 'path_set_from_environment_variables'})
    paths.set_installed_executables({executable.name: 'path_of_installed_executable'})

    assert paths.get_path_of(executable.name) == 'path_set_manually'


def test_path_from_command_line_argument_priority(paths, executable):
    paths.parse_command_line_arguments([executable.argument, 'path_set_from_command_line_arguments'])
    paths.set_environment_variables({executable.environment_variable: 'path_set_from_environment_variables'})
    paths.set_installed_executables({executable.name: 'path_of_installed_executable'})

    assert paths.get_path_of(executable.name) == 'path_set_from_command_line_arguments'


def test_path_from_environment_variables_priority(paths, executable):
    paths.set_environment_variables({executable.environment_variable: 'path_set_from_environment_variables'})
    paths.set_installed_executables({executable.name: 'path_of_installed_executable'})

    assert paths.get_path_of(executable.name) == 'path_set_from_environment_variables'
