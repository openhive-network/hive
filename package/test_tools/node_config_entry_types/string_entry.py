from .config_entry import ConfigEntry


class String(ConfigEntry):
    def parse_from_text(self, text):
        self._value = text.strip('"')
        return self._value

    def serialize_to_text(self):
        return f'"{self._value}"'

    @classmethod
    def _validate(cls, value):
        cls._validate_type(value, [str, type(None)])
