from .config_entry import ConfigEntry


class Untouched(ConfigEntry):
    def __init__(self):
        self.__value = None

    def parse_from_text(self, text):
        self.__value = text

    def serialize_to_text(self):
        return self.__value

    @property
    def value(self):
        return self.__value

    @value.setter
    def value(self, value):
        self.__value = value
