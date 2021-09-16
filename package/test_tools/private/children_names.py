from test_tools.exceptions import NameAlreadyInUse


class NameBaseNotSet(Exception):
    pass


class ChildrenNames:
    def __init__(self, name_base=None):
        self.names = []
        self.name_base = name_base
        self.next_name_numbers = {self.name_base: 0}

    def create_name(self, name_base=None):
        if self.name_base is None and name_base is None:
            raise NameBaseNotSet('Unable to create a name without name base')

        name_base = self.name_base if name_base is None else name_base
        if name_base not in self.next_name_numbers:
            self.next_name_numbers[name_base] = 0
        while True:
            name = f'{name_base}{self.next_name_numbers[name_base]}'
            if name not in self.names:
                break
            self.next_name_numbers[name_base] += 1

        self.names.append(name)
        self.next_name_numbers[name_base] += 1
        return name

    def register_name(self, name):
        if name in self.names:
            raise NameAlreadyInUse()

        self.names.append(name)
        return name
