class ChildrenNames:
    def __init__(self, name_base):
        self.name_base = name_base
        self.next_name_number = 0

    def create_name(self):
        name = f'{self.name_base}{self.next_name_number}'
        self.next_name_number += 1
        return name
