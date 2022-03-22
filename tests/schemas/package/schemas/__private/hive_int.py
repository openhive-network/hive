import re


class HiveIntMetaclass(type):
    @staticmethod
    def is_hive_int(value) -> bool:
        """
        Checks if passed value is Hive int (described below).

        In Hive ints are represented in two ways. If value can be stored in 32-bit int,
        then Hive represents it as int. When is too big (or too small) to be expressed
        as 32-bit int, then is expressed as string.

        So for example:
        1 is represented without any modifications as 1,
        but 100'000'000'000 will be expressed as string "100000000000".
        """

        if type(value) == int:
            return True

        if type(value) == str:
            if re.match(r'^-?\d+$', value):
                return True

        return False

    def __instancecheck__(self, instance):
        return self.is_hive_int(instance)


class HiveInt(metaclass=HiveIntMetaclass):
    pass
