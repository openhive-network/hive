from .config_entry import ConfigEntry


class Integer(ConfigEntry):
    def __init__(self):
        self.__value = None

    def parse_from_text(self, text):
        self.__value = int(text)

    def serialize_to_text(self):
        return str(self.__value)

    @property
    def value(self):
        return self.__value

    @value.setter
    def value(self, value):
        self.__value = value
