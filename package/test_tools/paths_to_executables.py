from argparse import ArgumentParser
from os import getenv, path
from pathlib import Path
import shutil

from test_tools.exceptions import MissingPathToExecutable, NotSupported


class _PathsToExecutables:
    BUILD_ROOT_PATH_COMMAND_LINE_ARGUMENT = '--build-root-path'
    BUILD_ROOT_PATH_ENVIRONMENT_VARIABLE = 'HIVE_BUILD_ROOT_PATH'

    class __ExecutableDetails:
        def __init__(self, name, default_path_from_build):
            self.name = name
            self.argument = f'--{name.replace("_", "-")}-path'
            self.environment_variable = f'{name}_PATH'.upper()
            self.default_path_from_build = default_path_from_build

    def __init__(self):
        self.paths = {}
        self.command_line_arguments = None
        self.environment_variables = None
        self.installed_executables = None
        self.supported_executables = [
            self.__ExecutableDetails('hived', 'programs/hived/hived'),
            self.__ExecutableDetails('cli_wallet', 'programs/cli_wallet/cli_wallet'),
            self.__ExecutableDetails('get_dev_key', 'programs/util/get_dev_key'),
            self.__ExecutableDetails('truncate_block_log', 'programs/util/truncate_block_log'),
        ]

        self.parse_command_line_arguments()
        self.set_environment_variables()
        self.set_installed_executables()

    def __is_supported(self, executable_name):
        return any(executable_name == executable.name for executable in self.supported_executables)

    def get_configuration_hint(self):
        return (
            f'Edit and add following line to /etc/environment and restart computer.\n'
            f'{self.BUILD_ROOT_PATH_ENVIRONMENT_VARIABLE}= # Should be something like: \"/home/dev/hive/build\"'
        )

    def print_paths_in_use(self):
        entries = []
        for executable in self.supported_executables:
            entries += [
                f'Name: {executable.name}\n'
                f'Path: {self.get_path_of(executable.name)}\n'
            ]
        print('\n'.join(entries))

    def get_paths_in_use(self):
        paths = {}
        for executable in self.supported_executables:
            try:
                path = self.get_path_of(executable.name)
            except MissingPathToExecutable:
                path = None

            paths[executable.name] = path
        return paths

    def get_path_of(self, executable_name):
        executable = self.__get_executable_details(executable_name)

        if executable_name in self.paths:
            return self.paths[executable_name]

        if getattr(self.command_line_arguments, executable_name) is not None:
            return getattr(self.command_line_arguments, executable_name)

        if self.__is_build_root_set_as_command_line_argument():
            return self.__get_path_relative_to_command_line_argument_build_root(executable)

        if executable.environment_variable in self.environment_variables:
            return self.environment_variables[executable.environment_variable]

        if self.__is_build_root_set_as_environment_variable():
            return self.__get_path_relative_to_environment_variable_build_root(executable)

        if executable_name in self.installed_executables and self.installed_executables[executable_name] is not None:
            return self.installed_executables[executable_name]

        raise MissingPathToExecutable(f'Missing path to {executable_name}\n' + self.get_configuration_hint())

    def __get_executable_details(self, executable_name):
        for executable in self.supported_executables:
            if executable.name == executable_name:
                return executable

        raise NotSupported(f'Executable {executable_name} is not supported')

    def __is_build_root_set_as_command_line_argument(self):
        return self.command_line_arguments.build_root is not None

    def __get_path_relative_to_command_line_argument_build_root(self, executable):
        build_root = Path(self.command_line_arguments.build_root)
        return str(build_root.joinpath(executable.default_path_from_build))

    def __is_build_root_set_as_environment_variable(self):
        return self.BUILD_ROOT_PATH_ENVIRONMENT_VARIABLE in self.environment_variables

    def __get_path_relative_to_environment_variable_build_root(self, executable):
        build_root = Path(self.environment_variables[self.BUILD_ROOT_PATH_ENVIRONMENT_VARIABLE])
        return str(build_root.joinpath(executable.default_path_from_build))

    def set_path_of(self, executable_name, executable_path):
        if not self.__is_supported(executable_name):
            raise NotSupported(f'Executable {executable_name} is not supported')

        self.paths[executable_name] = executable_path

    def parse_command_line_arguments(self, arguments=None):
        parser = ArgumentParser()

        parser.add_argument(self.BUILD_ROOT_PATH_COMMAND_LINE_ARGUMENT, dest='build_root')
        for executable in self.supported_executables:
            parser.add_argument(executable.argument, dest=executable.name)

        self.command_line_arguments, _ = parser.parse_known_args(arguments)

    def set_environment_variables(self, variables=None):
        if variables is None:
            variables = self.__get_environment_variables_from_operating_system()

        self.environment_variables = {name: path.expandvars(value) for name, value in variables.items()}

    def __get_environment_variables_from_operating_system(self):
        variables = {}

        self.__append_environment_variable_value_if_defined(variables, self.BUILD_ROOT_PATH_ENVIRONMENT_VARIABLE)

        for executable in self.supported_executables:
            self.__append_environment_variable_value_if_defined(variables, executable.environment_variable)

        return variables

    @staticmethod
    def __append_environment_variable_value_if_defined(variables, name):
        variable = getenv(name)
        if variable is not None:
            variables[name] = variable

    def set_installed_executables(self, installed_executables=None):
        self.installed_executables = {}

        if installed_executables is None:
            for executable in self.supported_executables:
                self.installed_executables[executable.name] = shutil.which(executable.name)
            return

        for executable in self.supported_executables:
            if executable.name in installed_executables.keys():
                self.installed_executables[executable.name] = installed_executables[executable.name]
            else:
                self.installed_executables[executable.name] = None


__paths = _PathsToExecutables()


def get_path_of(executable_name):
    return __paths.get_path_of(executable_name)


def set_path_of(executable_name, executable_path):
    __paths.set_path_of(executable_name, executable_path)


def get_paths_in_use():
    return __paths.get_paths_in_use()


def print_paths_in_use():
    __paths.print_paths_in_use()


def print_configuration_hint():
    print(__paths.get_configuration_hint())
