class PathsToExecutables:
    class __ExecutableDetails:
        def __init__(self, name):
            self.name = name
            self.argument = f'--{name.replace("_", "-")}-path'

    def __init__(self):
        self.paths = {}
        self.command_line_arguments = None
        self.supported_executables = [
            self.__ExecutableDetails('hived'),
            self.__ExecutableDetails('cli_wallet'),
            self.__ExecutableDetails('get_dev_key'),
        ]

        self.parse_command_line_arguments()

    def get_path_of(self, executable_name):
        if executable_name in self.paths:
            return self.paths[executable_name]

        if getattr(self.command_line_arguments, executable_name) is not None:
            return getattr(self.command_line_arguments, executable_name)

        raise Exception(f'Missing path to {executable_name}')

    def set_path_of(self, executable_name, executable_path):
        self.paths[executable_name] = executable_path

    def parse_command_line_arguments(self, arguments=None):
        from argparse import ArgumentParser
        parser = ArgumentParser()
        for executable in self.supported_executables:
            parser.add_argument(executable.argument, dest=executable.name)

        self.command_line_arguments, _ = parser.parse_known_args(arguments)
