"""
Two model sets generated from the same openapi.json in one generation path - the variants of Hive models of the
generator (`api_client_generator.hive_variants`) in the hiveio_api package.

- public: dataclasses (`hiveio_api._base.HiveModel`) with builtin types expressed by aliases from
  `schemas.fields.public` (e.g. `AccountName = str`) - returned by API calls, never validated,
- validation: msgspec models (`PreconfiguredBaseModel`) with validation forms of the types
  (`schemas.fields.validation`, e.g. `AccountName` validated by msgspec itself) - used only by explicit
  `validate_schema` calls, imported lazily.

Both use the `x-hive-type` annotations of openapi.json, they differ only in the types the annotations are mapped to.
Operations (`Hf26Operation`, `LegacyOperation`), transactions and members of operations returned by APIs too
(`Authority`, `Price`) are mapped to the `operations` module of each variant, generated from the description of
operations (see generate_operations.py).
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Final

from api_client_generator import hive_variants

from schemas.fields import public, validation

if TYPE_CHECKING:
    from pathlib import Path

    from api_client_generator.model_options import CustomTypes, ModelOptions

PUBLIC_BASE_CLASS: Final[str] = "hiveio_api._base.HiveModel"
"""`HiveModel` of its defining module - generated modules are imported by the generator before the package is complete."""


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

    def common_package(self, base_directory: Path) -> Path:
        """The package of models shared by several APIs."""
        return self.package_directory(base_directory) / "common"

    @property
    def common_import(self) -> str:
        return f"{self.package}.common"


OPERATION_TYPES: Final[dict[str, str]] = {
    "Hf26Operation": "hf26",
    "LegacyOperation": "legacy",
    "Transaction": "hf26",
    "TransactionLegacy": "legacy",
    "Authority": "values",
    "Price": "values",
    "PriceLegacy": "legacy",
}
"""
Models of the `operations` module of each variant used by APIs (`x-hive-type`) - unions of operations, transactions and
members of operations returned by APIs too (authorities, prices) - with the modules of the public package of operations
defining them (see generate_operations.py), imported directly by public models.
"""


PUBLIC: Final[ModelVariant] = ModelVariant(
    name="public",
    model_options=hive_variants.public_model_options(PUBLIC_BASE_CLASS),
    custom_types=hive_variants.public_custom_types(
        public.__all__,
        {name: f"hiveio_api.operations.{module}.{name}" for name, module in OPERATION_TYPES.items()},
    ),
    package="hiveio_api",
    public=True,
)

VALIDATION: Final[ModelVariant] = ModelVariant(
    name="validation",
    model_options=hive_variants.VALIDATION_MODEL_OPTIONS,
    custom_types=hive_variants.validation_custom_types(
        validation.__all__, {name: f"hiveio_api._validation.operations.{name}" for name in OPERATION_TYPES}
    ),
    package="hiveio_api._validation",
    public=False,
)

VARIANTS: Final[tuple[ModelVariant, ...]] = (PUBLIC, VALIDATION)
