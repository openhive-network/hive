from .config_entry import ConfigEntry


class NotSupported(Exception):
    pass


class List(ConfigEntry):
    class __ListWithoutAdditionOperator(list):
        def __iadd__(self, other):
            """Operator += is removed, because there is no -= operator.

            Consistent way of working with list config entries are methods:
            - to add element: append or extend
            - to remove element: remove
            """

            raise NotSupported(
                f'Operator += is removed. Use methods "{self.append.__name__}" or "{self.extend.__name__}" instead.'
            )

    def __init__(self, item_type, separator=' ', begin='', end='', single_line=True):
        self.__item_type = item_type

        super().__init__(self.__ListWithoutAdditionOperator())

        self.__separator = separator
        self.__begin = begin
        self.__end = end
        self.__single_line = single_line

    def clear(self):
        self.set_value([])

    def parse_from_text(self, text):
        import re
        match_result = re.match(fr'^\s*{re.escape(self.__begin)}(.*){re.escape(self.__end)}\s*$', text)
        # TODO: Raise if can't match

        for item_text in match_result[1].split(self.__separator):
            item = self.__item_type()
            parsed = item.parse_from_text(item_text)
            self._value.append(parsed)

        return self._value

    def serialize_to_text(self):
        def serialize_value(value):
            item = self.__item_type()
            item.set_value(value)
            return item.serialize_to_text()

        values = [serialize_value(value) for value in self._value]
        return self.__begin + self.__separator.join(values) + self.__end if self.__single_line else values

    def _validate(self, value):
        def check_single_value(single_value, item_type):
            if single_value is None:
                raise ValueError('You cannot store None in list')
            item_type._validate(single_value)

        if isinstance(value, list):
            for single_value in value:
                check_single_value(single_value, self.__item_type)
        else:
            try:
                check_single_value(value, self.__item_type)
            except ValueError as error:
                raise ValueError(
                    'To clear a list entry you have to write:\n'
                    '  config.entry = []\n'
                    'instead of:\n'
                    '  config.entry = None'
                ) from error

    def _set_value(self, value):
        if not isinstance(value, list):
            value = [value]

        self._value = self.__ListWithoutAdditionOperator(value)
