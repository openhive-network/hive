class ConfigEntry:
    def __init__(self):
        self._value = self._get_unset_value()
        self.__is_set = False

    def _parse_from_text(self, text):
        raise Exception('Not implemented')

    def _serialize_to_text(self):
        raise Exception('Not implemented')

    def _get_value(self):
        return self._value

    def _get_unset_value(self):
        return None

    def _set_value(self, value):
        self._value = value

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
        return self._get_value() if self.__is_set else self._get_unset_value()

    # Final method, shouldn't be overridden
    def set_value(self, value):
        self._set_value(value)
        self.__is_set = True

    # Final method, shouldn't be overridden
    def is_set(self):
        return self.__is_set

    # Final method, shouldn't be overridden
    def clear(self):
        self.__is_set = False
        self._value = self._get_unset_value()
