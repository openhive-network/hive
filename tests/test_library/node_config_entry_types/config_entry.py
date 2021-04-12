class ConfigEntry:
    def parse_from_text(self, text):
        raise Exception('Not implemented')

    def serialize_to_text(self):
        raise Exception('Not implemented')

    @property
    def value(self):
        raise Exception('Not implemented')

    @value.setter
    def value(self, value):
        raise Exception('Not implemented')
