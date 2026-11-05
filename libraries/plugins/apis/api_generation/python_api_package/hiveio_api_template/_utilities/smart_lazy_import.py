"""
Deprecated - the objects of `hiveio_api_client._utilities.smart_lazy_import` under their former path in hiveio_api,
kept for packages importing it. Use `hiveio_api_client._utilities.smart_lazy_import` instead.
"""

from __future__ import annotations

from hiveio_api_client._utilities.smart_lazy_import import (
    AggregatedAliasedImportInput,
    AggregatedModuleInterfaceMappingInput,
    Alias,
    AliasedImportInput,
    AliasMapping,
    GetattrProtocol,
    ImportInput,
    ModuleComponent,
    ModuleGlobals,
    ModuleInterfaceMapping,
    ModuleInterfaceMappingInput,
    ModulePath,
    _extract_aliases,
    _validate_all_in_translations,
    aggregate_same_import,
    lazy_module_factory,
    smart_lazy_getattr,
)

__all__ = [
    "AggregatedAliasedImportInput",
    "AggregatedModuleInterfaceMappingInput",
    "Alias",
    "AliasMapping",
    "AliasedImportInput",
    "GetattrProtocol",
    "ImportInput",
    "ModuleComponent",
    "ModuleGlobals",
    "ModuleInterfaceMapping",
    "ModuleInterfaceMappingInput",
    "ModulePath",
    "_extract_aliases",
    "_validate_all_in_translations",
    "aggregate_same_import",
    "lazy_module_factory",
    "smart_lazy_getattr",
]
