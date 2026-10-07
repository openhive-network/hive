"""
Lazy imports of module attributes.

A package `__init__.py` defines `__getattr__` mapping exported names to the modules defining them, so a module is
imported only when one of its names is accessed for the first time.

Usage:
```python
from hiveio_api._utilities.smart_lazy_import import aggregate_same_import, lazy_module_factory

__all__ = ["DatabaseApi", "FindAccountsResponse"]

__getattr__ = lazy_module_factory(
    globals(),
    *aggregate_same_import("DatabaseApi", module="hiveio_api.database_api.database_api_client"),
    *aggregate_same_import("FindAccountsResponse", module="hiveio_api.database_api.database_api_description"),
)
```
"""

from __future__ import annotations

import importlib
from typing import Any, Callable, get_args

__all__ = [
    "aggregate_same_import",
    "lazy_module_factory",
    "smart_lazy_getattr",
]


ModuleComponent = str
ModulePath = str
Alias = str

ImportInput = tuple[ModulePath, ModuleComponent]
AliasedImportInput = tuple[ModulePath, ModuleComponent, Alias]
AggregatedAliasedImportInput = tuple[ModuleComponent, Alias]

ModuleInterfaceMappingInput = ImportInput | AliasedImportInput
AggregatedModuleInterfaceMappingInput = ModuleComponent | AggregatedAliasedImportInput

ModuleInterfaceMapping = dict[ModuleComponent, ModulePath]
AliasMapping = dict[Alias, ModuleComponent]

ModuleGlobals = dict[str, Any]
GetattrProtocol = Callable[[str], Any]


def smart_lazy_getattr(
    name: str, module_globals: ModuleGlobals, name_module_translation: ModuleInterfaceMapping, aliases: AliasMapping
) -> Any:
    """
    Import the attribute from the module it is mapped to.

    Args:
        name: Name of the attribute to import
        module_globals: Globals of the module the attribute is accessed on (`globals()`)
        name_module_translation: Mapping of attribute names to paths of modules defining them
        aliases: Mapping of alias names to target names

    Returns:
        Imported attribute

    Raises:
        AttributeError: If the attribute was not found
    """
    if name in ("__path__", "__file__", "__all__", "__name__"):
        if name == "__path__":
            name = "__file__"
        return module_globals[name]

    if name not in name_module_translation:
        if name in aliases:
            name = aliases[name]
        else:
            raise AttributeError(f"Module '{module_globals['__name__']}' has no attribute '{name}'")

    return importlib.import_module(name_module_translation[name]).__getattribute__(name)


def lazy_module_factory(
    module_globals: ModuleGlobals, /, *translations: ModuleInterfaceMappingInput
) -> GetattrProtocol:
    """
    Create module `__getattr__` importing exported attributes lazily.

    Args:
        module_globals: Globals of the module (`globals()`), all names from its `__all__` have to be translated
        *translations: Tuples `(module_path, attribute_name)` or `(module_path, attribute_name, alias)`

    Returns:
        Function to be assigned to the module `__getattr__`.

    Example:
    ```python
    __getattr__ = lazy_module_factory(
        globals(),
        ("hiveio_api.database_api.database_api_client", "DatabaseApi"),
        ("hiveio_api.database_api.database_api_client", "DatabaseApi", "DatabaseApiAlias"),
    )
    ```
    """
    translations_no_alias, aliases = _extract_aliases(translations)
    _validate_all_in_translations(module_globals, translations_no_alias, aliases)

    def new_getattr(name: str) -> Any:
        return smart_lazy_getattr(name, module_globals, translations_no_alias, aliases)

    return new_getattr


def aggregate_same_import(
    *attributes: AggregatedModuleInterfaceMappingInput, module: ModulePath
) -> tuple[ModuleInterfaceMappingInput, ...]:
    """
    Helper function to aggregate multiple attributes from the same module.

    Args:
        module: Module path
        *attributes: Attribute names to import from the module

    Returns:
        A mapping of attribute names to their module paths
    """
    result: list[ModuleInterfaceMappingInput] = []
    for attribute in attributes:
        if isinstance(attribute, str):
            result.append((module, attribute))
        else:
            result.append((module, *attribute))
    return tuple(result)


def _validate_all_in_translations(
    module_globals: ModuleGlobals, translations: ModuleInterfaceMapping, aliases: AliasMapping
) -> None:
    missing = set(module_globals["__all__"]) - (set(translations.keys()) | set(aliases.keys()))
    if missing:
        raise ImportError(f"Missing translations for: {', '.join(missing)}")


def _extract_aliases(
    translation: tuple[ModuleInterfaceMappingInput, ...],
) -> tuple[ModuleInterfaceMapping, AliasMapping]:
    translations: ModuleInterfaceMapping = {}
    aliases: AliasMapping = {}

    for item in translation:
        if len(item) == len(get_args(ImportInput)):  # simple import
            translations[item[1]] = item[0]
        elif len(item) == len(get_args(AliasedImportInput)):  # aliased import
            translations[item[1]] = item[0]
            aliases[item[2]] = item[1]  # type: ignore[misc]
        else:
            raise ImportError(f"Invalid input during lazy import definition: {item}")

    return translations, aliases
