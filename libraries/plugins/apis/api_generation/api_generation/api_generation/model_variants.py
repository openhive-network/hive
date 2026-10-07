"""
Two model sets generated from the same openapi.json in one generation path.

- public: frozen dataclasses (`hiveio_api._base.HiveModel`) with builtin types expressed by aliases from
  `schemas.fields.base_type_mappings` (e.g. `AccountName = str`) - returned by API calls, never validated,
- validation: msgspec models (`PreconfiguredBaseModel`) with validating types from `schemas.fields` - used only by
  explicit `validate_schema` calls, imported lazily.

Both use the `x-hive-type` annotations of openapi.json, they differ only in the types the annotations are mapped to.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Final

from api_client_generator.model_options import CustomTypes, ModelOptions

from schemas.fields import base_type_mappings
from schemas.fields.base_type_mappings import BASE_TYPE_MAPPINGS

if TYPE_CHECKING:
    from pathlib import Path

HIVE_TYPE_EXTENSION: Final[str] = "x-hive-type"
FORMAT_TYPES: Final[dict[str, str]] = {"date-time": "HiveDateTime"}

PUBLIC_BASE_CLASS: Final[str] = "hiveio_api._base.HiveModel"
VALIDATION_BASE_CLASS: Final[str] = "schemas._preconfigured_base_model.PreconfiguredBaseModel"


@dataclass(frozen=True)
class ModelVariant:
    name: str
    model_options: ModelOptions
    custom_types: CustomTypes
    package: str
    """Dotted path of the package holding generated modules, e.g. `hiveio_api._validation`."""
    public: bool

    def description_file(self, base_directory: Path, api: str) -> Path:
        package_directory = self.package_directory(base_directory)
        if self.public:
            return package_directory / api / f"{api}_description.py"
        return package_directory / f"{api}.py"

    def package_directory(self, base_directory: Path) -> Path:
        return base_directory.joinpath("python_api_package", *self.package.split("."))

    @property
    def common_import(self) -> str:
        return f"{self.package}.common"


def _custom_types(types: dict[str, str], *, skip_in_unions: bool = False) -> CustomTypes:
    return CustomTypes(
        types=types, extension=HIVE_TYPE_EXTENSION, format_types=FORMAT_TYPES, skip_in_unions=skip_in_unions
    )


PUBLIC: Final[ModelVariant] = ModelVariant(
    name="public",
    # enums as Literal - values of public models are plain builtins (no conversion when built from JSON)
    model_options=ModelOptions(
        model_type="dataclass", base_class=PUBLIC_BASE_CLASS, frozen=True, kw_only=True, enum_as_literal=True
    ),
    custom_types=_custom_types({name: f"{base_type_mappings.__name__}.{name}" for name in BASE_TYPE_MAPPINGS}),
    package="hiveio_api",
    public=True,
)

VALIDATION: Final[ModelVariant] = ModelVariant(
    name="validation",
    model_options=ModelOptions(
        model_type="msgspec", base_class=VALIDATION_BASE_CLASS, kw_only=True, enum_as_literal=True
    ),
    # msgspec does not support unions of custom types with other types - such members keep their builtin type
    custom_types=_custom_types(dict(BASE_TYPE_MAPPINGS), skip_in_unions=True),
    package="hiveio_api._validation",
    public=False,
)

VARIANTS: Final[tuple[ModelVariant, ...]] = (PUBLIC, VALIDATION)
