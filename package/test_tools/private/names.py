from typing import Set

from test_tools.exceptions import NameAlreadyInUse


class Names:
    def __init__(self):
        self.__unique_names = set()
        self.__next_name_numbers = {}

    def get_names_in_use(self) -> Set[str]:
        numbered_names = set()
        for name, number_limit in self.__next_name_numbers.items():
            numbered_names.update([f'{name}{i}' for i in range(number_limit)])

        return self.__unique_names | numbered_names

    def register_numbered_name(self, name: str) -> str:
        if name not in self.__next_name_numbers:
            self.__next_name_numbers[name] = 0

        name_with_number = f'{name}{self.__next_name_numbers[name]}'
        self.__next_name_numbers[name] += 1

        return name_with_number

    def register_unique_name(self, name: str):
        if name in self.__unique_names:
            raise NameAlreadyInUse(f'Name "{name}" is already in use')

        self.__unique_names.add(name)
