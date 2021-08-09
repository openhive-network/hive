from test_tools.private.node_config_entry_types.config_entry import ConfigEntry
from test_tools.exceptions import ParseError


class Boolean(ConfigEntry):
    def __init__(self, value=None, *, true='1', false='0'):
        super().__init__(value)
        self.__true_serialized = true
        self.__false_serialized = false

    def parse_from_text(self, text):
        if text == self.__true_serialized:
            self.set_value(True)
        elif text == self.__false_serialized:
            self.set_value(False)
        else:
            raise ParseError(
                f'Can\'t parse bool value from: {text}. '
                f'Available values are: {self.__true_serialized} and {self.__false_serialized}.'
            )

        return self._value

    def serialize_to_text(self):
        return self.__true_serialized if self._value else self.__false_serialized

    @classmethod
    def _validate(cls, value):
        cls._validate_type(value, [bool, type(None)])
