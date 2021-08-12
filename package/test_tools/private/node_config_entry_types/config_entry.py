class ConfigEntry:
    def __init__(self, value=None):
        self._value = None  # To disable the warning about definition outside of __init__
        self.set_value(value)

    def clear(self):
        self.set_value(None)

    def parse_from_text(self, text):
        raise NotImplementedError()

    def serialize_to_text(self):
        raise NotImplementedError()

    def get_value(self):
        return self._value

    def set_value(self, value):
        try:
            self._validate(value)
        except TypeError as error:
            raise ValueError(str(error)) from error

        self._set_value(value)

    def _set_value(self, value):
        self._value = value

    @classmethod
    def validate(cls, value):
        return cls._validate(value)

    @classmethod
    def _validate(cls, value):
        '''Raises exception if value or its type is incorrect.

        Must be overriden by derived classes.'''
        raise NotImplementedError()

    @classmethod
    def _validate_type(cls, value, valid_types):
        if type(value) not in valid_types:
            raise TypeError(f'{valid_types} were expected, but {repr(value)} with type {type(value)} was passed')

    def __repr__(self):
        return f'{self.__class__.__name__}({self.get_value()!r})'
