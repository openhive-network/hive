from __future__ import annotations

from typing import ClassVar, cast, get_args

import msgspec

from hiveio_api.validation.errors import FieldValidationError

__all__ = [
    "SimpleInitValidator",
]


class SimpleInitValidator:
    """
    Base of user-friendly field types of builtins - checked when created by their form of validation models.

    A type derives also from its builtin and gives the form (`Annotated[<builtin>, msgspec.Meta(...)]` of
    `hiveio_api.fields.validation`) - the rules are defined once and msgspec checks them in both places:

        class AccountName(SimpleInitValidator, str, form=validation.AccountName): ...

    The value is made by the builtin (`Uint16t("1")` is `1`) and checked as it is kept.
    """

    _form: ClassVar[object]

    def __init_subclass__(cls, *, form: object = None, **kwargs: object) -> None:
        super().__init_subclass__(**kwargs)
        if form is not None:
            cls._form = form

    def __init__(self, value: object) -> None:
        try:
            msgspec.convert(self, self._form)
        except msgspec.ValidationError as error:
            raise FieldValidationError(f"`{value}` is not a valid `{type(self).__name__}`: {error}") from error

    @classmethod
    def meta(cls) -> msgspec.Meta:
        """Rules of the type (e.g. `Uint16t.meta().le`) - the `msgspec.Meta` of its form."""
        meta: msgspec.Meta = get_args(cls._form)[1]
        return meta

    @classmethod
    def covered_type(cls) -> type[str | int]:
        """The builtin of the type (`str`, `int`) - its JSON form."""
        covered_type: type[str | int] = get_args(cls._form)[0]
        return covered_type

    def to_builtins(self) -> str | int:
        """The JSON form - the value as its builtin (msgspec does not encode subclasses of builtins, `enc_hook_hf26`)."""
        return self.covered_type()(cast("str | int", self))  # subclasses derive from their builtin
