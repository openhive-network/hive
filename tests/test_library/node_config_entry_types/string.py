from .config_entry import ConfigEntry


class String(ConfigEntry):
    def _parse_from_text(self, text):
        self._value = text.strip('"')

    def _serialize_to_text(self):
        return f'"{self._value}"'
