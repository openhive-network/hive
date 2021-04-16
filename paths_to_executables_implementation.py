class PathsToExecutables:
    def __init__(self):
        self.paths = {}

    def get_path_of(self, executable_name):
        if executable_name in self.paths:
            return self.paths[executable_name]

        raise Exception(f'Missing path to {executable_name}')

    def set_path_of(self, executable_name, executable_path):
        self.paths[executable_name] = executable_path
