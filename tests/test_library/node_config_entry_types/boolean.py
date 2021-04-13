from .config_entry import ConfigEntry


class Boolean(ConfigEntry):
    def _parse_from_text(self, text):
        self._value = int(text) == 1

    def _serialize_to_text(self):
        return '1' if self._value else '0'
