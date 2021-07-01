from .config_entry import ConfigEntry


class Boolean(ConfigEntry):
    def parse_from_text(self, text):
        self.set_value(int(text) == 1)
        return self._value

    def serialize_to_text(self):
        return '1' if self._value else '0'

    @classmethod
    def _validate(cls, value):
        cls._validate_type(value, [bool, type(None)])
