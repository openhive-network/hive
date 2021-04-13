from .config_entry import ConfigEntry


class Integer(ConfigEntry):
    def _parse_from_text(self, text):
        self._value = int(text)

    def _serialize_to_text(self):
        return str(self._value)
