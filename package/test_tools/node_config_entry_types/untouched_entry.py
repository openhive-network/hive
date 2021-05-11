from .config_entry import ConfigEntry


class Untouched(ConfigEntry):
    class _ValueProxyType(ConfigEntry._ValueProxy, str):
        def __new__(cls, entry):
            return str.__new__(cls, entry._value)

        def __hash__(self):
            return str.__hash__(self)

    _ValueProxy = _ValueProxyType

    def _parse_from_text(self, text):
        self._value = text
        return self._value

    def _serialize_to_text(self):
        return self._value
