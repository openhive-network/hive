"""
User-friendly asset of public models - one type for both JSON forms of assets.

`Asset` is built from either form - legacy (`"1.000 HIVE"`) or NAI (`{"amount": "1000", "precision": 3, "nai":
"@@000000021"}`) - and remembers it, so `to_builtins` gives back the form it came in. Assets are created with the kind
of asset:

- `Asset.hive.satoshi(1001)` - 1.001 HIVE given in the smallest units,
- `Asset.hbd.tokens(1.001)` - 1.001 HBD given in tokens,
- `Asset.vests.from_str("1.000000 VESTS")` - from the legacy form,
- `Asset.hive.factory(amount=1001, nai="@@000000021", precision=3)` - from members of the NAI form.

Kinds of assets are defined only by the information about them (`hiveio_api.fields.public._asset_info`: NAI, precision,
symbols). Assets, kinds and their factories of assets (`Asset.hive.satoshi`, `Asset.hive.tokens`) give the information
about the kind with `token()` (displayed as the symbol, e.g. `f"{Asset.hive.satoshi.token()}"` is `HIVE`) - so a factory
stands for its kind (e.g. a parameter of a test creating assets).

Assets of the same kind are added (also by `sum()`), subtracted, compared and divided (`/` gives the ratio of amounts),
multiplied and divided by integers (`/` truncates to the smallest unit towards zero, `//` rounds down). As the former
asset types, they are also compared with integers of the smallest units (`asset > 0`) and copied with another amount
(`asset.copy(amount=1000)`).

Assets created by the kinds are in the NAI form. The form sent to an API is decided by the parameters of the endpoint,
not by the asset (`as_nai`, `as_legacy`), and symbols of the legacy form by the network of the API (`TESTS`, `TBD` on
the testnet). The asset remembers the network it was created for (`Asset.hive.tokens(10, testnet=True)`, or the symbol
of the legacy form) only for local displaying (`str`, `as_legacy`, `token`).

This module must stay import-light (no msgspec, no other hiveio_api modules than the import-light `_asset_info`) -
generated public models use it.
"""

from __future__ import annotations

import re
from collections.abc import Mapping
from dataclasses import dataclass
from decimal import Decimal
from typing import TYPE_CHECKING, ClassVar, Final, NoReturn, Self, overload

from hiveio_api.fields.public._asset_info import (
    HBD_ASSET,
    HIVE_ASSET,
    KNOWN_ASSETS,
    VESTS_ASSET,
    asset_info_of_nai,
    asset_info_of_symbol,
)

if TYPE_CHECKING:
    from collections.abc import Callable

    from hiveio_api.fields.public._asset_info import AssetInfo

__all__ = [
    "Asset",
    "AssetFactory",
    "AssetKind",
    "AssetNaiAmount",
    "SatoshiFactory",
    "TokensFactory",
]

_LEGACY_PATTERN: Final = re.compile(r"^(-?)(\d+)(?:\.(\d+))? ([A-Z]+)$")
_AMOUNT_PATTERN: Final = re.compile(r"-?[0-9]+")
_NAI_PATTERN: Final = re.compile(r"@@[0-9]{9}")
_MAX_PRECISION: Final[int] = 12
"""Maximal number of decimal places of assets in hived (`HIVE_ASSET_MAX_DECIMALS`)."""

AssetNaiAmount = int
"""Amount of the NAI form in the smallest units (`Asset.amount`)."""


@dataclass(frozen=True)
class AssetKind:
    """Kind of asset (HIVE, HBD, VESTS) - creates assets of this kind, in the NAI form."""

    name: str
    """Name of the kind in `Asset` (`Asset.hive`)."""
    info: AssetInfo
    """Precision, NAI and symbols of the kind (`HIVE_ASSET`, ...)."""

    @property
    def satoshi(self) -> SatoshiFactory:
        """Creates assets of the amount given in the smallest units, e.g. `Asset.hive.satoshi(1001)` is 1.001 HIVE."""
        return SatoshiFactory(self)

    @property
    def tokens(self) -> TokensFactory:
        """Creates assets of the amount given in tokens, e.g. `Asset.hbd.tokens(1.001)` is 1.001 HBD."""
        return TokensFactory(self)

    def token(self, testnet: bool = False) -> AssetInfo:
        """Information about the kind, displayed with the symbol of the network, e.g. `HIVE` (`TESTS` of the testnet)."""
        return self.info.of_network(testnet=testnet)

    def from_str(self, text: str) -> Asset:
        """Asset of this kind from the legacy form, e.g. `Asset.vests.from_str("1.000000 VESTS")`."""
        asset = Asset.from_str(text)
        if asset.nai != self.info.nai:
            raise ValueError(f"`{text}` is not an asset of {self.info.get_symbol()}")
        return asset

    def factory(self, *, amount: int | str, nai: str | None = None, precision: int | None = None) -> Asset:
        """Asset from members of the NAI form - `nai` and `precision` have to match the kind when given."""
        info = self.info
        if nai is not None and nai != info.nai:
            raise ValueError(f"NAI of {info.get_symbol()} is {info.nai}, not {nai}")
        if precision is not None and precision != info.precision:
            raise ValueError(f"Precision of {info.get_symbol()} is {info.precision}, not {precision}")
        return Asset.from_nai({"amount": amount, "precision": info.precision, "nai": info.nai})


@dataclass(frozen=True)
class AssetFactory:
    """
    Creates assets of a kind when called with the amount - and names the kind (`token`).

    A factory stands for its kind where assets of the kind are created and named, e.g. a parameter of a test:
    `currency(5)` creates the asset and `currency.token()` names it.
    """

    kind: AssetKind

    _NAME: ClassVar[str]
    """Name of the factory in the kind (`Asset.hive.satoshi`)."""

    def token(self, testnet: bool = False) -> AssetInfo:
        """Information about the kind, displayed with the symbol of the network, e.g. `HIVE` (`TESTS` of the testnet)."""
        return self.kind.token(testnet)

    def __repr__(self) -> str:
        return f"Asset.{self.kind.name}.{self._NAME}"


class SatoshiFactory(AssetFactory):
    """Creates assets of the amount given in the smallest units, e.g. `Asset.hive.satoshi(1001)` is 1.001 HIVE."""

    _NAME: ClassVar[str] = "satoshi"

    def __call__(self, amount: int, *, testnet: bool = False) -> Asset:
        """
        Asset of the amount given in the smallest units.

        Args:
            amount: Amount in the smallest units.
            testnet: The asset is displayed with the symbol of the testnet (`TESTS`, `TBD`), see the module.

        Raises:
            TypeError: When the amount is not an int.
        """
        if isinstance(amount, bool) or not isinstance(amount, int):
            raise TypeError(f"Amount in satoshi has to be an int, not {type(amount).__name__}: {amount!r}")
        info = self.kind.info
        return Asset._create(satoshi=amount, precision=info.precision, nai=info.nai, testnet=testnet)


class TokensFactory(AssetFactory):
    """Creates assets of the amount given in tokens, e.g. `Asset.hbd.tokens(1.001)` is 1.001 HBD."""

    _NAME: ClassVar[str] = "tokens"

    def __call__(self, amount: int | float | Decimal | str, *, testnet: bool = False) -> Asset:
        """
        Asset of the amount given in tokens.

        Args:
            amount: Amount in tokens.
            testnet: The asset is displayed with the symbol of the testnet (`TESTS`, `TBD`), see the module.

        Raises:
            TypeError: When the amount is not a number nor a string.
            ValueError: When the amount is not a finite number or has more decimal places than the kind.
        """
        info = self.kind.info
        if isinstance(amount, bool) or not isinstance(amount, int | float | Decimal | str):
            raise TypeError(f"Amount in tokens has to be a number, not {type(amount).__name__}: {amount!r}")
        try:
            satoshi = (Decimal(str(amount)) if isinstance(amount, float) else Decimal(amount)).scaleb(info.precision)
        except ArithmeticError as error:  # not a number (`decimal.InvalidOperation`), too big (`decimal.Overflow`)
            raise ValueError(f"`{amount}` is not an amount of {info.get_symbol()}") from error
        if not satoshi.is_finite():
            raise ValueError(f"`{amount}` is not an amount of {info.get_symbol()} - it has to be finite")
        if satoshi != satoshi.to_integral_value():
            raise ValueError(f"{amount} {info.get_symbol()} has more than {info.precision} decimal places")
        return self.kind.satoshi(int(satoshi), testnet=testnet)


class Asset:
    """
    Amount of an asset (HIVE, HBD, VESTS) - immutable, compared by value whatever its form.

    Created by kinds of assets (`Asset.hive`, `Asset.hbd`, `Asset.vests`) or from JSON (`Asset("1.000 HIVE")`,
    `Asset({"amount": "1000", "precision": 3, "nai": "@@000000021"})`).
    """

    hive: ClassVar[AssetKind] = AssetKind(name="hive", info=HIVE_ASSET)
    hbd: ClassVar[AssetKind] = AssetKind(name="hbd", info=HBD_ASSET)
    vests: ClassVar[AssetKind] = AssetKind(name="vests", info=VESTS_ASSET)

    __slots__ = ("_legacy", "_nai", "_precision", "_satoshi", "_testnet")

    _satoshi: int
    _precision: int
    _nai: str
    _legacy: bool
    """Created from the legacy form - serialized back to it by `to_builtins`."""
    _testnet: bool
    """Displayed with symbols of the testnet (see the module)."""

    def __init__(self, value: str | Mapping[str, object]) -> None:
        """Build the asset from JSON - the legacy form (string) or the NAI form (object), as `from_builtins`."""
        asset = self.from_builtins(value)
        self.__set(asset._satoshi, asset._precision, asset._nai, legacy=asset._legacy, testnet=asset._testnet)

    @classmethod
    def _create(cls, *, satoshi: int, precision: int, nai: str, legacy: bool = False, testnet: bool = False) -> Self:
        asset = object.__new__(cls)
        asset.__set(satoshi, precision, nai, legacy=legacy, testnet=testnet)
        return asset

    def __set(self, satoshi: int, precision: int, nai: str, *, legacy: bool, testnet: bool) -> None:
        object.__setattr__(self, "_satoshi", satoshi)
        object.__setattr__(self, "_precision", precision)
        object.__setattr__(self, "_nai", nai)
        object.__setattr__(self, "_legacy", legacy)
        object.__setattr__(self, "_testnet", testnet)

    @classmethod
    def kind_of(cls, info: AssetInfo) -> AssetKind:
        """The kind of assets of the information, e.g. `Asset.hive` of `HIVE_ASSET`."""
        for kind in (cls.hive, cls.hbd, cls.vests):
            if kind.info == info:
                return kind
        raise ValueError(f"No kind of assets has the information {info!r}")

    @classmethod
    def from_builtins(cls, value: object) -> Self:
        """Build the asset from JSON builtins - the legacy form (string) or the NAI form (object)."""
        if isinstance(value, str):
            return cls.from_str(value)
        if isinstance(value, Mapping):
            return cls.from_nai(value)
        raise TypeError(f"Asset can be built from a string or an object, not {type(value).__name__}: {value!r}")

    @classmethod
    def from_str(cls, text: str) -> Self:
        """Asset from the legacy form, e.g. `"1.000 HIVE"` (testnet symbols `TESTS` / `TBD` too)."""
        match = _LEGACY_PATTERN.match(text)
        if match is None:
            raise ValueError(f"`{text}` is not an asset in the legacy form, e.g. `1.000 HIVE`")
        sign, whole, fraction, symbol = match.groups()
        info = asset_info_of_symbol(symbol)
        if info is None:
            known = dict.fromkeys(known_symbol for known in KNOWN_ASSETS for known_symbol in known.symbol)
            raise ValueError(f"`{text}` is not an asset - unknown symbol, known: {', '.join(known)}")
        fraction = fraction or ""
        if len(fraction) != info.precision:
            raise ValueError(
                f"`{text}` is not an asset of {info.get_symbol()} - it has {info.precision} decimal places"
            )
        return cls._create(
            satoshi=int(f"{sign}{whole}{fraction}"),
            precision=info.precision,
            nai=info.nai,
            legacy=True,
            testnet=info.testnet,
        )

    @classmethod
    def from_nai(cls, value: Mapping[str, object]) -> Self:
        """Asset from the NAI form, e.g. `{"amount": "1000", "precision": 3, "nai": "@@000000021"}`."""
        if not isinstance(value, Mapping):
            raise TypeError(f"NAI form of an asset is an object, not {type(value).__name__}: {value!r}")
        amount, precision, nai = value.get("amount"), value.get("precision"), value.get("nai")
        if isinstance(amount, bool) or not isinstance(amount, int | str) or not _AMOUNT_PATTERN.fullmatch(str(amount)):
            cls.__invalid_nai(value, "`amount` has to be an integer or a string of digits")
        if isinstance(precision, bool) or not isinstance(precision, int) or not 0 <= precision <= _MAX_PRECISION:
            cls.__invalid_nai(value, f"`precision` has to be an integer from 0 to {_MAX_PRECISION}")
        if not isinstance(nai, str) or _NAI_PATTERN.fullmatch(nai) is None:
            cls.__invalid_nai(value, "`nai` has to be a string of `@@` and 9 digits")
        if set(value) != {"amount", "precision", "nai"}:
            cls.__invalid_nai(value, "it has to have exactly `amount`, `precision` and `nai`")
        info = asset_info_of_nai(nai)
        if info is not None and precision != info.precision:
            cls.__invalid_nai(value, f"precision of {info.get_symbol()} is {info.precision}")
        return cls._create(satoshi=int(amount), precision=precision, nai=nai)

    @staticmethod
    def __invalid_nai(value: Mapping[str, object], reason: str) -> NoReturn:
        raise ValueError(f"`{dict(value)}` is not an asset in the NAI form - {reason}")

    @property
    def amount(self) -> AssetNaiAmount:
        """Amount in the smallest units (e.g. 1001 for 1.001 HIVE) - `amount` of the NAI form."""
        return self._satoshi

    @property
    def tokens(self) -> Decimal:
        """Amount in tokens (e.g. `Decimal("1.001")` for 1.001 HIVE)."""
        return Decimal(self._satoshi).scaleb(-self._precision)

    @property
    def precision(self) -> int:
        """Number of decimal places of the kind (e.g. 3 for HIVE) - `precision` of the NAI form."""
        return self._precision

    @property
    def nai(self) -> str:
        """NAI of the kind (e.g. `@@000000021` for HIVE) - `nai` of the NAI form."""
        return self._nai

    @property
    def symbol(self) -> str:
        """Symbol of the asset on the mainnet (`HIVE`, `HBD`, `VESTS`); NAI for assets of other kinds."""
        info = asset_info_of_nai(self._nai)
        return self._nai if info is None else info.get_symbol()

    @property
    def is_legacy(self) -> bool:
        """The asset was created from the legacy form - it is serialized back to it (`to_builtins`)."""
        return self._legacy

    def is_hive(self) -> bool:
        """The asset is of HIVE (`TESTS` on the testnet)."""
        return self._nai == HIVE_ASSET.nai

    def is_hbd(self) -> bool:
        """The asset is of HBD (`TBD` on the testnet)."""
        return self._nai == HBD_ASSET.nai

    def is_vests(self) -> bool:
        """The asset is of VESTS."""
        return self._nai == VESTS_ASSET.nai

    def as_nai(self) -> dict[str, object]:
        """The NAI form, e.g. `{"amount": "1001", "precision": 3, "nai": "@@000000021"}`."""
        return {"amount": str(self._satoshi), "precision": self._precision, "nai": self._nai}

    def as_legacy(self, *, testnet: bool | None = None) -> str:
        """
        The legacy form, e.g. `"1.001 HIVE"`.

        Args:
            testnet: Symbol of the testnet (`TESTS`, `TBD`) or of the mainnet. By default of the network the asset was
                created for (see the module).
        """
        return f"{self.pretty_amount()} {self.token(testnet)}"

    def token(self, testnet: bool | None = None) -> AssetInfo:
        """
        Information about the kind of the asset, displayed with the symbol of the legacy form, e.g. `TESTS`.

        Args:
            testnet: Information of the testnet or of the mainnet - as in `as_legacy`.

        Raises:
            ValueError: When the asset is of no known kind.
        """
        info = asset_info_of_nai(self._nai)
        if info is None:
            raise ValueError(f"Asset of NAI {self._nai} is of no known kind - it has no information and no legacy form")
        return info.of_network(testnet=self._testnet if testnet is None else testnet)

    def pretty_amount(self) -> str:
        """Amount in tokens with all decimal places, e.g. `1.001` - the legacy form without the symbol."""
        sign = "-" if self._satoshi < 0 else ""
        digits = str(abs(self._satoshi)).rjust(self._precision + 1, "0")
        if not self._precision:
            return f"{sign}{digits}"
        return f"{sign}{digits[: -self._precision]}.{digits[-self._precision :]}"

    def as_float(self) -> float:
        """Amount in tokens as a float (e.g. `1.001`) - not exact, for calculations."""
        return float(self.tokens)

    def as_serialized_nai(self) -> dict[str, object]:
        """The NAI form (as `as_nai`)."""
        return self.as_nai()

    def to_builtins(self) -> str | dict[str, object]:
        """JSON builtins of the asset in the form it was created in."""
        return self.as_legacy() if self.is_legacy else self.as_nai()

    def __setattr__(self, name: str, value: object) -> NoReturn:
        raise AttributeError(f"{type(self).__name__} is immutable")

    def __reduce__(self) -> tuple[Callable[[int, int, str, bool, bool], Self], tuple[int, int, str, bool, bool]]:
        """Pickled as its members (`__setattr__` rejects the default restoring of state)."""
        return type(self)._unpickled, (self._satoshi, self._precision, self._nai, self._legacy, self._testnet)

    @classmethod
    def _unpickled(cls, satoshi: int, precision: int, nai: str, legacy: bool, testnet: bool) -> Self:
        return cls._create(satoshi=satoshi, precision=precision, nai=nai, legacy=legacy, testnet=testnet)

    def __copy__(self) -> Self:
        return self  # immutable

    def __deepcopy__(self, memo: dict[int, object]) -> Self:
        return self  # immutable

    @classmethod
    def from_asset_like(cls, value: object) -> Asset | None:
        """
        The asset given in any form, None when the value is not an asset.

        Accepted: `Asset`, JSON of an asset (`"1.000 HIVE"`, an object of the NAI form), objects converting themselves
        to the NAI form (`as_nai()`) and objects with members of the NAI form (`amount`, `precision`, `nai`, e.g.
        protobuf assets of wax).
        """
        if isinstance(value, Asset):
            return value
        try:
            if isinstance(value, str | Mapping):
                return cls.from_builtins(value)
            as_nai = getattr(value, "as_nai", None)
            if callable(as_nai):
                nai_form = as_nai()
                return cls.from_nai(nai_form) if isinstance(nai_form, Mapping) else None
            members = {name: getattr(value, name, None) for name in ("amount", "precision", "nai")}
            if all(member is not None and not callable(member) for member in members.values()):
                return cls.from_nai(members)
        except (TypeError, ValueError):
            return None
        return None

    def __eq__(self, other: object) -> bool:
        """
        Equal to assets of the same amount and kind, whatever their form - also given in any other form of assets.

        The other value is converted to an asset first (`from_asset_like`, e.g. `"1.000 HIVE"`); an integer is the amount
        in the smallest units (`asset == 0`), as by the former asset types; values which are not assets are not equal.
        Hashes of the other forms differ from the hash of the asset - do not mix them as keys.
        """
        if isinstance(other, int) and not isinstance(other, bool):
            return self._satoshi == other
        asset = self.from_asset_like(other)
        if asset is None:
            return NotImplemented
        return self.__key() == asset.__key()

    def __hash__(self) -> int:
        return hash(self.__key())

    def __key(self) -> tuple[int, int, str]:
        return self._satoshi, self._precision, self._nai

    def __same_kind(self, other: object, operation: str) -> Asset:
        """The other asset (also in any other form of assets) - of the same kind."""
        asset = self.from_asset_like(other)
        if asset is None:
            raise TypeError(f"Cannot {operation} {self!r} and {type(other).__name__}: {other!r}")
        if (asset._nai, asset._precision) != (self._nai, self._precision):
            raise TypeError(f"Cannot {operation} assets of different kinds: {self} and {asset}")
        return asset

    def __with_satoshi(self, satoshi: int) -> Asset:
        return type(self)._create(
            satoshi=satoshi, precision=self._precision, nai=self._nai, legacy=self._legacy, testnet=self._testnet
        )

    def __add__(self, other: object) -> Asset:
        return self.__with_satoshi(self._satoshi + self.__same_kind(other, "add")._satoshi)

    def __sub__(self, other: object) -> Asset:
        return self.__with_satoshi(self._satoshi - self.__same_kind(other, "subtract")._satoshi)

    def __radd__(self, other: object) -> Asset:
        if type(other) is int and other == 0:  # the start of `sum()`
            return self
        return self + other

    def __rsub__(self, other: object) -> Asset:
        return -(self - other)

    def __mul__(self, other: int) -> Asset:
        if isinstance(other, bool) or not isinstance(other, int):
            return NotImplemented
        return self.__with_satoshi(self._satoshi * other)

    __rmul__ = __mul__

    def __floordiv__(self, other: int) -> Asset:
        """The asset divided by the number, rounded down to the smallest unit (e.g. 0.010 HIVE // 3 is 0.003 HIVE)."""
        if isinstance(other, bool) or not isinstance(other, int):
            return NotImplemented
        return self.__with_satoshi(self._satoshi // other)

    @overload
    def __truediv__(self, other: int) -> Asset: ...

    @overload
    def __truediv__(self, other: Asset | str | Mapping[str, object]) -> Decimal: ...

    def __truediv__(self, other: object) -> Asset | Decimal:
        """
        The asset divided by a number or the ratio of amounts of assets of the same kind (in any form).

        Divided by a number, the amount is truncated to the smallest unit towards zero, as by the former asset types
        (0.010 HIVE / 3 is 0.003 HIVE, -0.010 HIVE / 3 is -0.003 HIVE). Divided by an asset, the result is the ratio of
        their amounts (1.000 HIVE / 0.500 HIVE is 2).
        """
        if isinstance(other, int) and not isinstance(other, bool):
            if other == 0:
                raise ZeroDivisionError(f"Cannot divide {self!r} by zero")
            quotient = abs(self._satoshi) // abs(other)
            return self.__with_satoshi(quotient if (self._satoshi < 0) == (other < 0) else -quotient)
        return Decimal(self._satoshi) / Decimal(self.__same_kind(other, "divide")._satoshi)

    def __neg__(self) -> Asset:
        return self.__with_satoshi(-self._satoshi)

    def __abs__(self) -> Asset:
        return self.__with_satoshi(abs(self._satoshi))

    def __lt__(self, other: object) -> bool:
        return self._satoshi < self.__compared_satoshi(other)

    def __le__(self, other: object) -> bool:
        return self._satoshi <= self.__compared_satoshi(other)

    def __gt__(self, other: object) -> bool:
        return self._satoshi > self.__compared_satoshi(other)

    def __ge__(self, other: object) -> bool:
        return self._satoshi >= self.__compared_satoshi(other)

    def __compared_satoshi(self, other: object) -> int:
        """
        The amount compared with - of an asset of the same kind (in any form) or an integer of the smallest units.

        Integers are compared as by the former asset types (`asset > 0`).
        """
        if isinstance(other, int) and not isinstance(other, bool):
            return other
        return self.__same_kind(other, "compare")._satoshi

    def copy(self, *, amount: int | Asset | None = None) -> Asset:
        """
        The asset with another amount - in the smallest units or of an asset of the same kind (a copy when not given).

        As by the former asset types; the form and the network of the asset are kept.

        Raises:
            TypeError: When the amount is not an integer nor an asset of the same kind.
        """
        if amount is None:
            return self.__with_satoshi(self._satoshi)
        if isinstance(amount, int) and not isinstance(amount, bool):
            return self.__with_satoshi(amount)
        if isinstance(amount, Asset):
            return self.__with_satoshi(self.__same_kind(amount, "copy the amount of")._satoshi)
        raise TypeError(f"The amount of a copy of {self!r} has to be an int or an asset, not {type(amount).__name__}")

    def __str__(self) -> str:
        """The legacy form, e.g. `1.001 HIVE`; the NAI form for assets of other kinds."""
        return self.as_legacy() if asset_info_of_nai(self._nai) is not None else str(self.as_nai())

    def __repr__(self) -> str:
        info = asset_info_of_nai(self._nai)
        if self.is_legacy or info is None:
            return f"Asset({self.to_builtins()!r})"
        testnet = ", testnet=True" if self._testnet else ""
        return f"Asset.{self.kind_of(info).name}.satoshi({self._satoshi}{testnet})"
