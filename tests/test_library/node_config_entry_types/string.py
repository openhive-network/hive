from .config_entry import ConfigEntry


class String(ConfigEntry):
    def __init__(self):
        self.__value = None

    def parse_from_text(self, text):
        self.__value = text.strip('"')

    def serialize_to_text(self):
        return f'"{self.__value}"'

    @property
    def value(self):
        return self.__value

    @value.setter
    def value(self, value):
        self.__value = value
