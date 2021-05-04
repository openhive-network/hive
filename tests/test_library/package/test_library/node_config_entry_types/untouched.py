from .config_entry import ConfigEntry


class Untouched(ConfigEntry):
    def _parse_from_text(self, text):
        self._value = text

    def _serialize_to_text(self):
        return self._value
