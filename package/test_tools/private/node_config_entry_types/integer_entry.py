from test_tools.private.node_config_entry_types.config_entry import ConfigEntry


class Integer(ConfigEntry):
    def parse_from_text(self, text):
        self.set_value(int(text))
        return self._value

    def serialize_to_text(self):
        return str(self._value)

    @classmethod
    def _validate(cls, value):
        cls._validate_type(value, [int, type(None)])
