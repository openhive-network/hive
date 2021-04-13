from .config_entry import ConfigEntry


class List(ConfigEntry):
    def __init__(self, item_type, separator=' ', begin='', end='', single_line=True):
        super().__init__()

        self.__item_type = item_type

        self.__separator = separator
        self.__begin = begin
        self.__end = end
        self.__single_line = single_line

    def _get_unset_value(self):
        return []

    def _parse_from_text(self, text):
        self._value = []

        import re
        match_result = re.match(fr'^\s*{re.escape(self.__begin)}(.*){re.escape(self.__end)}\s*$', text)
        # TODO: Raise if can't match

        for item_text in match_result[1].split(self.__separator):
            item = self.__item_type()
            item.parse_from_text(item_text)
            self._value.append(item.get_value())

    def _serialize_to_text(self):
        def serialize_value(value):
            item = self.__item_type()
            item.set_value(value)
            return item.serialize_to_text()

        values = [serialize_value(value) for value in self._value]
        return self.__begin + self.__separator.join(values) + self.__end if self.__single_line else values
