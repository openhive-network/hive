class PathsToExecutables:
    class __ExecutableDetails:
        def __init__(self, name):
            self.name = name

    def __init__(self):
        self.paths = {}
        self.supported_executables = [
            self.__ExecutableDetails('hived'),
            self.__ExecutableDetails('cli_wallet'),
            self.__ExecutableDetails('get_dev_key'),
        ]

    def get_path_of(self, executable_name):
        if executable_name in self.paths:
            return self.paths[executable_name]

        raise Exception(f'Missing path to {executable_name}')

    def set_path_of(self, executable_name, executable_path):
        self.paths[executable_name] = executable_path
