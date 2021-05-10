class ConfigEntry:
    class _BaseProxyType:
        def __init__(self, entry):
            self.entry = entry

        def clear(self):
            self.entry.clear()

    class _UnsetProxyType(_BaseProxyType):
        @staticmethod
        def is_set():
            return False

        def __eq__(self, other):
            return isinstance(other, self.__class__)

    class _ValueProxyType(_BaseProxyType):
        @staticmethod
        def is_set():
            return True

        def __eq__(self, other):
            return self.entry._value == other

    _UnsetProxy = _UnsetProxyType
    _ValueProxy = _ValueProxyType

    def __init__(self):
        self.__is_set = False

    def _is_set(self):
        return self.__is_set

    def _parse_from_text(self, text):
        """Parse value from given text, store it inside entry and return."""
        raise Exception('Not implemented')

    def _serialize_to_text(self):
        raise Exception('Not implemented')

    def _create_unset_proxy(self):
        return self._UnsetProxy(self)

    def _create_value_proxy(self):
        return self._ValueProxy(self)

    def _set_value(self, value):
        self._value = value

    def _clear(self):
        self.__is_set = False

    # Final method, shouldn't be overridden
    def is_set(self):
        return self._is_set()

    # Final method, shouldn't be overridden
    # TODO: When there will be Python 3.8 or newer use @final instead of above comment
    def parse_from_text(self, text):
        parsed = self._parse_from_text(text)
        self.__is_set = True
        return parsed

    # Final method, shouldn't be overridden
    def serialize_to_text(self):
        return self._serialize_to_text()

    # Final method, shouldn't be overridden
    def get_value(self):
        return self._create_value_proxy() if self.is_set() else self._create_unset_proxy()

    # Final method, shouldn't be overridden
    def set_value(self, value):
        self._set_value(value)
        self.__is_set = True

    # Final method, shouldn't be overridden
    def clear(self):
        self._clear()
