from __future__ import annotations

import subprocess
import sys
import typing
from typing import Final

import pytest
from hiveio_api.fields import public, validation
from hiveio_api.fields.hive_datetime import HiveDateTime
from hiveio_api.fields.public import _fields as public_aliases

from tests.generated_package import PACKAGE_ROOT

FIELD_TYPES: Final[list[str]] = sorted(public_aliases.__all__)
"""Names of the field types - aliases of `hiveio_api.fields.public`."""
NAI_ASSETS: Final[list[str]] = ["AnyAsset", "AssetHbd", "AssetHive", "AssetHiveOrHbd", "AssetHiveOrVests", "AssetVests"]
LEGACY_ASSETS: Final[list[str]] = [f"Legacy{name}" for name in NAI_ASSETS]
INTEGERS_OF_64_BITS: Final[list[str]] = ["HiveInt", "Int64t", "Uint64t"]
OF_BUILTINS: Final[list[str]] = sorted(
    set(FIELD_TYPES) - {*NAI_ASSETS, *LEGACY_ASSETS, *INTEGERS_OF_64_BITS, "HiveDateTime"}
)


def test_package_exports_every_alias() -> None:
    # ACT & ASSERT
    assert set(public.__all__) >= set(FIELD_TYPES)


@pytest.mark.parametrize("name", FIELD_TYPES)
def test_alias_has_the_form_of_the_same_name(name: str) -> None:
    # ACT & ASSERT
    assert name in validation.__all__


def builtin_of(form: object) -> object:
    """The builtin a form is made of - of its first member for unions (e.g. `int | Annotated[str, ...]`)."""
    if typing.get_origin(form) is typing.Union:
        form = typing.get_args(form)[0]
    return typing.get_args(form)[0] if typing.get_origin(form) is typing.Annotated else form


@pytest.mark.parametrize("name", OF_BUILTINS)
def test_alias_is_the_builtin_of_the_form(name: str) -> None:
    # ACT
    alias = getattr(public, name)

    # ASSERT
    assert alias is builtin_of(getattr(validation, name))


@pytest.mark.parametrize("name", INTEGERS_OF_64_BITS)
def test_alias_of_integers_of_64_bits_tells_their_form(name: str) -> None:
    # ACT
    alias = getattr(public, name)

    # ASSERT
    assert alias == typing.Annotated[int, public.IntegerForm.NUMBER_OR_STRING]
    assert builtin_of(getattr(validation, name)) is int


def test_alias_of_dates_is_the_user_friendly_date() -> None:
    # ACT & ASSERT
    assert public.HiveDateTime is HiveDateTime


@pytest.mark.parametrize(
    ("name", "legacy"), [(name, False) for name in NAI_ASSETS] + [(name, True) for name in LEGACY_ASSETS]
)
def test_assets_are_mapped_to_types_of_their_form(name: str, *, legacy: bool) -> None:
    # ACT
    form = getattr(validation, name)

    # ASSERT
    if legacy:
        assert typing.get_origin(form) is typing.Annotated
        assert typing.get_args(form)[0] is str
    else:
        assert isinstance(form, type)
        assert issubclass(form, validation.AnyAsset)
    expected_form = public.AssetForm.LEGACY if legacy else public.AssetForm.NAI
    assert getattr(public, name) == typing.Annotated[public.Asset, expected_form]


@pytest.mark.parametrize(
    "class_",
    [
        public.Asset,
        public.AssetFactory,
        public.AssetForm,
        public.AssetInfo,
        public.AssetKind,
        public.IntegerForm,
        public.SatoshiFactory,
        public.TokensFactory,
    ],
)
def test_classes_are_of_the_package_not_of_its_private_modules(class_: type) -> None:
    """Generators import types from their module (`__module__`) - classes are imported from the package."""
    # ACT & ASSERT
    assert class_.__module__ == public.__name__


def test_package_does_not_import_msgspec() -> None:
    # ACT
    result = subprocess.run(
        [
            sys.executable,
            "-c",
            f"import sys; sys.path.insert(0, {str(PACKAGE_ROOT)!r}); import hiveio_api.fields.public; "
            "print(sorted(m for m in sys.modules if m.startswith(('msgspec', 'hiveio_api.fields', 'hiveio_api.validation'))))",
        ],
        capture_output=True,
        text=True,
        check=True,
    )

    # ASSERT
    assert result.stdout.strip() == (
        "['hiveio_api.fields', 'hiveio_api.fields.hive_constants', 'hiveio_api.fields.hive_datetime', "
        "'hiveio_api.fields.public', 'hiveio_api.fields.public._asset_info', 'hiveio_api.fields.public._assets', "
        "'hiveio_api.fields.public._fields', 'hiveio_api.fields.serializable']"
    )
