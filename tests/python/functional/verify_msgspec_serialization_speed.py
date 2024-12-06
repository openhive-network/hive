from __future__ import annotations

import random
import re
import string
from typing import Annotated, Final

import msgspec as ms

ACCOUNT_NAME_SEGMENT_REGEX: Final[str] = r"[a-z][a-z0-9\-]+[a-z0-9]"
BASE_58_REGEX: Final[str] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"


# AccountName = Annotated[str, ms.Meta(min_length=3, max_length=16, pattern=rf"^{ACCOUNT_NAME_SEGMENT_REGEX}(?:\.{ACCOUNT_NAME_SEGMENT_REGEX})*$")]

Int16t = Annotated[int, ms.Meta(ge=-32768, le=32767)]
DEFAULT_WEIGHT: Final[Int16t] = Int16t(0)


# @dataclass
# class VoteOperation:
#     __operation_name__ = "vote"
#     __offset__ = 0

#     voter: AccountName
#     author: AccountName
#     permlink: str
#     weight: Int16t = DEFAULT_WEIGHT

# Funkcja generująca pojedynczy segment nazwy

AccountNameImpl = Annotated[
    str,
    ms.Meta(min_length=3, max_length=16, pattern=rf"^{ACCOUNT_NAME_SEGMENT_REGEX}(?:\.{ACCOUNT_NAME_SEGMENT_REGEX})*$"),
]


class ValidateAccountName(ms.Struct):
    val: AccountNameImpl

    def __post_init__(self):
        assert len(self.val) > 3
        assert len(self.val) < 16
        assert re.match(
            rf"^{ACCOUNT_NAME_SEGMENT_REGEX}(?:\.{ACCOUNT_NAME_SEGMENT_REGEX})*$", self.val
        ), f"Invalid account name: {self.val}"


def AccountNameMsgspec(value: str) -> AccountNameImpl:
    return ValidateAccountName(val=value).val


def generate_random_strings(count=1_000_000, length=10):
    """
    Generuje listę losowych stringów o podanej długości.

    :param count: Liczba stringów do wygenerowania (domyślnie 1_000_000)
    :param length: Długość każdego stringa (domyślnie 10)
    :return: Lista stringów
    """
    return ["".join(random.choices(string.ascii_lowercase, k=length)) for _ in range(count)]


if __name__ == "__main__":
    mln_str_list = generate_random_strings()

    # #pydantic
    # for acc_name in mln_str_list:
    #     AccountName.validate(AccountName(acc_name))

    # msgspec
    for acc_name in mln_str_list:
        AccountNameMsgspec(acc_name)
