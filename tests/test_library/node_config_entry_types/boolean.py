from .config_entry import ConfigEntry


class Boolean(ConfigEntry):
    def __init__(self):
        self.__value = None

    def parse_from_text(self, text):
        self.__value = int(text) == 1

    def serialize_to_text(self):
        return '1' if self.__value else '0'

    @property
    def value(self):
        return self.__value

    @value.setter
    def value(self, value):
        self.__value = value
