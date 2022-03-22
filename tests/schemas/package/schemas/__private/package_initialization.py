import pykwalify.types

from schemas.__private.hive_int import HiveInt


def initialize() -> None:
    # Add new type -- hive_int -- to pykwalify types. This solution is hacky,
    # but there is no satisfying built-in solution and pykwalify extensions
    # do not solve the problem.
    pykwalify.types.tt.update({'hive_int': HiveInt.is_hive_int})
    pykwalify.types._types.update({'hive_int': HiveInt})
