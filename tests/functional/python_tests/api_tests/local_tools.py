class BaseArgs:
    def as_array(self) -> list:
        return list(self.__dict__.values())

    def as_dict(self) -> dict:
        return self.__dict__
