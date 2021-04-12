from .config_entry import ConfigEntry


class List(ConfigEntry):
    def __init__(self, item_type, separator=' ', begin='', end='', single_line=True):
        self.__item_type = item_type
        self.__values = None

        self.__separator = separator
        self.__begin = begin
        self.__end = end
        self.__single_line = single_line

    def parse_from_text(self, text):
        self.__values = []

        import re
        match_result = re.match(fr'^\s*{re.escape(self.__begin)}(.*){re.escape(self.__end)}\s*$', text)
        # TODO: Raise if can't match

        for item_text in match_result[1].split(self.__separator):
            item = self.__item_type()
            item.parse_from_text(item_text)
            self.__values.append(item.value)

    def serialize_to_text(self):
        def serialize_value(value):
            item = self.__item_type()
            item.value = value
            return item.serialize_to_text()

        values = [serialize_value(value) for value in self.__values]
        return self.__begin + self.__separator.join(values) + self.__end if self.__single_line else values

    @property
    def value(self):
        return self.__values

    @value.setter
    def value(self, value):
        self.__values = value

    def __len__(self):
        return len(self.__values)

    def __iter__(self):
        return self.value.__iter__()

    def __eq__(self, other):
        if isinstance(other, List):
            other = other.value
        elif not isinstance(other, list):
            raise Exception('Comparison with unsupported type')

        return self.value == other

    def __ne__(self, other):
        return not self == other
