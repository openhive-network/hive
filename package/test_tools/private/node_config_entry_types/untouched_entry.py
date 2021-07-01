from test_tools.private.node_config_entry_types.config_entry import ConfigEntry


class Untouched(ConfigEntry):
    def parse_from_text(self, text):
        self._value = text
        return self._value

    def serialize_to_text(self):
        return self._value

    @classmethod
    def _validate(cls, value):
        cls._validate_type(value, [str, type(None)])
