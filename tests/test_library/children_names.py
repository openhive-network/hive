class NameBaseNotSet(Exception):
    pass


class NameAlreadyInUse(Exception):
    pass


class ChildrenNames:
    def __init__(self, name_base=None):
        self.names = []
        self.name_base = name_base
        self.next_name_number = 0

    def create_name(self, name_base=None):
        if self.name_base is None and name_base is None:
            raise NameBaseNotSet('Unable to create a name without name base')

        while True:
            name = f'{self.name_base}{self.next_name_number}'
            if name not in self.names:
                break
            self.next_name_number += 1

        self.names.append(name)
        self.next_name_number += 1
        return name

    def register_name(self, name):
        if name in self.names:
            raise NameAlreadyInUse()

        self.names.append(name)
        return name
