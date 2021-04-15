from .config_entry import ConfigEntry


class Boolean(ConfigEntry):
    def _parse_from_text(self, text):
        self._value = int(text) == 1

    def _serialize_to_text(self):
        return '1' if self._value else '0'

    def _set_value(self, value):
        if type(value) is not bool:
            raise TypeError(f'bool was expected, but {value} with type {type(value)} was passed')

        self._value = value
