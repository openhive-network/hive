class NotSupported(Exception):
    pass


class PathsToExecutables:
    class __ExecutableDetails:
        def __init__(self, name):
            self.name = name
            self.argument = f'--{name.replace("_", "-")}-path'
            self.environment_variable = f'{name}_PATH'.upper()

    def __init__(self):
        self.paths = {}
        self.command_line_arguments = None
        self.environment_variables = None
        self.supported_executables = [
            self.__ExecutableDetails('hived'),
            self.__ExecutableDetails('cli_wallet'),
            self.__ExecutableDetails('get_dev_key'),
        ]

        self.parse_command_line_arguments()
        self.set_environment_variables()

    def __is_supported(self, executable_name):
        return any([executable_name == executable.name for executable in self.supported_executables])

    def get_path_of(self, executable_name):
        if not self.__is_supported(executable_name):
            raise NotSupported(f'Executable {executable_name} is not supported')

        if executable_name in self.paths:
            return self.paths[executable_name]

        if getattr(self.command_line_arguments, executable_name) is not None:
            return getattr(self.command_line_arguments, executable_name)

        if executable_name in self.environment_variables and self.environment_variables[executable_name] is not None:
            return self.environment_variables[executable_name]

        raise Exception(f'Missing path to {executable_name}')

    def set_path_of(self, executable_name, executable_path):
        if not self.__is_supported(executable_name):
            raise NotSupported(f'Executable {executable_name} is not supported')

        self.paths[executable_name] = executable_path

    def parse_command_line_arguments(self, arguments=None):
        from argparse import ArgumentParser
        parser = ArgumentParser()
        for executable in self.supported_executables:
            parser.add_argument(executable.argument, dest=executable.name)

        self.command_line_arguments, _ = parser.parse_known_args(arguments)

    def set_environment_variables(self, variables=None):
        self.environment_variables = {}

        if variables is None:
            import os
            for executable in self.supported_executables:
                self.environment_variables[executable.name] = os.getenv(executable.environment_variable)
            return

        for executable in self.supported_executables:
            if executable.environment_variable in variables.keys():
                self.environment_variables[executable.name] = variables[executable.environment_variable]
            else:
                self.environment_variables[executable.name] = None
