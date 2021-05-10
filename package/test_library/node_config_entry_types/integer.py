from .config_entry import ConfigEntry


class Integer(ConfigEntry):
    class _ValueProxyType(ConfigEntry._ValueProxy, int):
        def __new__(cls, entry):
            return int.__new__(cls, entry._value)

    _ValueProxy = _ValueProxyType

    def _parse_from_text(self, text):
        self._value = int(text)

    def _serialize_to_text(self):
        return str(self._value)

    def _set_value(self, value):
        if type(value) is not int:
            raise TypeError(f'int was expected, but {value} with type {type(value)} was passed')

        self._value = value
