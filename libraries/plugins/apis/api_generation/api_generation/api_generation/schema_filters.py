"""
Schema filters of public models - masks of fields to validate (`set_validation_mask` of endpoints), seen only by type
checkers.

For each public model (with fields) reachable from results of endpoints a `TypedDict` `<Model>Filter` is generated in the package
`hiveio_api._schema_filters` of stubs only (`.pyi`, a module per module of models: models of `hiveio_api.common.block`
have filters in `hiveio_api/_schema_filters/common/block.pyi`). The model gets the base
`ClassFilterInterface["_filters.<Model>Filter"]` of hiveio_api_client, `_filters` being its stub module imported only by
type checkers - so the filters are no part of the runtime of the package nor of its public interface.

A key of a filter per field of the model: `bool`, or `<Filter> | bool` for a field holding one model (also optional,
items of lists, values of dicts). Unions of several models (operations) take only `bool` - validated whole, as by
`hiveio_api.validation._masks`, which reduces validation models by the same rules.
"""

from __future__ import annotations

import ast
import dataclasses
import importlib
import sys
import types
from collections import defaultdict
from typing import TYPE_CHECKING, Annotated, Final, Union, get_args, get_origin, get_type_hints

if TYPE_CHECKING:
    from collections.abc import Iterable, Mapping, Sequence
    from pathlib import Path

FILTERS_PACKAGE: Final[str] = "hiveio_api._schema_filters"
FILTER_SUFFIX: Final[str] = "Filter"
FILTERS_ALIAS: Final[str] = "_filters"
"""Name of the stub module of filters imported by modules of models (only by type checkers)."""
BASE_CLASS: Final[str] = "ClassFilterInterface"
BASE_CLASS_SOURCE: Final[str] = "hiveio_api_client"

_FILTERS_IMPORT_COMMENT: Final[str] = (
    "# schema filters - stubs only, used by type arguments (strings) of bases of models (`noqa` keeps the import in"
    " tidying)\n# pyright: reportMissingModuleSource=false\n"
)
_PACKAGE_STUB: Final[str] = (
    "# Schema filters of models - masks of fields to validate (`set_validation_mask` of endpoints). Stubs only, seen\n"
    "# by type checkers: models refer to their filters by `ClassFilterInterface[...]`, filters are no part of the\n"
    "# public interface of the package.\n"
)


def add_schema_filters(base_directory: Path, apis: Sequence[str]) -> list[Path]:
    """
    Generate schema filters of public models reachable from results of endpoints of the APIs, give the models their bases.

    Args:
        base_directory: The base directory containing the api_generation folder.
        apis: Names of the APIs - their public descriptions are generated.

    Returns:
        Files written - stubs of filters and modules of models given the bases (to be tidied).
    """
    package_root = base_directory / "python_api_package"
    if str(package_root) not in sys.path:
        sys.path.insert(0, str(package_root))
    template_directory = package_root / "hiveio_api_template"

    models = _models_of_results(apis, package_root, template_directory)
    by_module: dict[str, list[type]] = defaultdict(list)
    for model in models:
        by_module[model.__module__].append(model)

    written: list[Path] = []
    filters_root = package_root.joinpath(*FILTERS_PACKAGE.split("."))
    filters_root.mkdir(parents=True, exist_ok=True)
    (filters_root / "__init__.pyi").write_text(_PACKAGE_STUB)
    written.append(filters_root / "__init__.pyi")
    for module_name, module_models in sorted(by_module.items()):
        stub = _stub_path(package_root, module_name)
        stub.parent.mkdir(parents=True, exist_ok=True)
        for directory in stub.relative_to(filters_root).parents:
            init = filters_root / directory / "__init__.pyi"
            if not init.exists():
                init.write_text("")
                written.append(init)
        module_file = package_root.joinpath(*module_name.split(".")).with_suffix(".py")
        stub.write_text(_filters_stub(module_models, models, module_file))
        _add_bases(module_file, module_name, module_models)
        written.extend((stub, module_file))
    return written


def filter_name(model: type) -> str:
    """Name of the schema filter of the model."""
    return f"{model.__name__}{FILTER_SUFFIX}"


def nested_model(annotation: object, is_model: type | None = None) -> type | None:
    """
    The model a field of the given type holds - the model itself, optional, items of a list (set, variable tuple), values
    of a dict - `None` for other types (values, unions of several models).

    Args:
        annotation: Type of the field.
        is_model: Base class of models (`hiveio_api._base.HiveModel` by default).
    """
    base = is_model if is_model is not None else _hive_model()
    if isinstance(annotation, type) and dataclasses.is_dataclass(annotation) and issubclass(annotation, base):
        return annotation
    origin = get_origin(annotation)
    args = get_args(annotation)
    if origin is Annotated:
        return nested_model(args[0], base)
    if origin in (Union, types.UnionType):
        values = [arg for arg in args if arg is not type(None)]
        return nested_model(values[0], base) if len(values) == 1 else None
    if origin in (list, set, frozenset) and len(args) == 1:
        return nested_model(args[0], base)
    if origin is tuple and len(args) == 2 and args[1] is Ellipsis:  # `tuple[item, ...]`
        return nested_model(args[0], base)
    if origin is dict and len(args) == 2:  # `dict[key, value]`
        return nested_model(args[1], base)
    return None


def _hive_model() -> type:
    from hiveio_api._base import HiveModel

    return HiveModel


def _models_of_results(apis: Iterable[str], package_root: Path, template_directory: Path) -> set[type]:
    """Generated public models reachable from results of endpoints of the APIs through fields holding one model."""
    found: set[type] = set()
    pending: list[type] = []
    for api in apis:
        api_module = api.replace("-", "_")
        module = importlib.import_module(f"hiveio_api.{api_module}.{api_module}_description")
        description: Mapping[str, Mapping[str, Mapping[str, object]]] = getattr(module, f"{api_module}_description")
        for endpoints in description.values():
            for endpoint in endpoints.values():
                model = nested_model(endpoint.get("result"))
                if model is not None:
                    pending.append(model)
    while pending:
        model = pending.pop()
        if model in found or not _is_generated(model, package_root, template_directory):
            continue
        if not dataclasses.fields(model):  # nothing to select (e.g. empty results) - `set_validation_mask` not offered
            continue
        found.add(model)
        pending.extend(
            nested for annotation in get_type_hints(model).values() if (nested := nested_model(annotation)) is not None
        )
    return found


def _is_generated(model: type, package_root: Path, template_directory: Path) -> bool:
    """The model is defined by a generated module - not by a hand-written one of the template (e.g. `Asset`)."""
    parts = model.__module__.split(".")
    if parts[0] != "hiveio_api":
        return False
    relative = package_root.joinpath(*parts).with_suffix(".py").relative_to(package_root / "hiveio_api")
    return not (template_directory / relative).exists()


def _stub_path(package_root: Path, module_name: str) -> Path:
    """`hiveio_api.common.block` -> `<package root>/hiveio_api/_schema_filters/common/block.pyi`."""
    relative = module_name.split(".")[1:]
    return package_root.joinpath(*FILTERS_PACKAGE.split("."), *relative).with_suffix(".pyi")


def _stub_module(module_name: str) -> str:
    """`hiveio_api.common.block` -> `hiveio_api._schema_filters.common.block`."""
    return ".".join([FILTERS_PACKAGE, *module_name.split(".")[1:]])


def _filters_stub(module_models: Sequence[type], all_models: set[type], module_file: Path) -> str:
    """The stub module with filters of the models of a module of models."""
    docstrings = _field_docstrings(module_file)
    local = {filter_name(model) for model in module_models}
    imports: dict[str, set[str]] = defaultdict(set)
    imported: dict[str, str] = {}  # names in the stub by (module, name)
    classes: list[str] = []
    for model in sorted(module_models, key=lambda model: model.__name__):
        lines = [
            f"class {filter_name(model)}(TypedDict, total=False):",
            f'    """Fields of `{model.__name__}` to validate - `True` the whole field, a filter only its fields."""',
        ]
        hints = get_type_hints(model)
        for field in dataclasses.fields(model):
            nested = nested_model(hints[field.name])
            value = "bool"
            if nested is not None and nested in all_models:
                value = f"{_filter_reference(nested, model.__module__, local, imports, imported)} | bool"
            lines.append(f"    {field.name}: {value}")
            if docstring := docstrings.get((model.__name__, field.name)):
                lines.append(f'    """{docstring}"""')
        classes.append("\n".join(lines))
    header = ["from typing import TypedDict"]
    header.extend(f"from {module} import {', '.join(sorted(names))}" for module, names in sorted(imports.items()))
    return "\n".join(header) + "\n\n" + "\n\n".join(classes) + "\n"


def _filter_reference(
    nested: type, module_name: str, local: set[str], imports: dict[str, set[str]], imported: dict[str, str]
) -> str:
    """Name of the filter of the nested model in the stub of the module - imported from its stub when defined elsewhere."""
    name = filter_name(nested)
    if nested.__module__ == module_name:
        return name
    stub_module = _stub_module(nested.__module__)
    key = f"{stub_module}.{name}"
    if key not in imported:
        taken = local | set(imported.values())
        alias = name if name not in taken else f"{name}_{nested.__module__.rsplit('.', 1)[-1]}"
        imports[stub_module].add(name if alias == name else f"{name} as {alias}")
        imported[key] = alias
    return imported[key]


def _field_docstrings(module_file: Path) -> dict[tuple[str, str], str]:
    """Docstrings of fields of models of the module (string literals after them) - by names of models and fields."""
    docstrings: dict[tuple[str, str], str] = {}
    for node in ast.parse(module_file.read_text()).body:
        if not isinstance(node, ast.ClassDef):
            continue
        for statement, following in zip(node.body, node.body[1:], strict=False):
            if not (isinstance(statement, ast.AnnAssign) and isinstance(statement.target, ast.Name)):
                continue
            if isinstance(following, ast.Expr) and isinstance(following.value, ast.Constant):
                text = following.value.value
                if isinstance(text, str) and text.strip():
                    docstrings[node.name, statement.target.id] = _docstring_literal_body(text)
    return docstrings


def _docstring_literal_body(text: str) -> str:
    """The text as the body of a one-line `\"\"\"...\"\"\"` literal - whitespace joined, quotes and backslashes escaped."""
    body = " ".join(text.split()).replace("\\", "\\\\").replace('"""', '\\"\\"\\"')
    return f"{body} " if body.endswith('"') else body


def _add_bases(module_file: Path, module_name: str, models: Sequence[type]) -> None:
    """Give the models the base `ClassFilterInterface["_filters.<Model>Filter"]`, import their stub for type checkers."""
    names = {model.__name__ for model in models}
    lines = module_file.read_text().splitlines(keepends=True)
    module = ast.parse("".join(lines))
    classes = [node for node in module.body if isinstance(node, ast.ClassDef) and node.name in names]
    missing = names - {node.name for node in classes}
    assert not missing, f"Models {sorted(missing)} are not defined by {module_file}"
    for node in sorted(
        classes, key=lambda node: (node.bases[-1].end_lineno, node.bases[-1].end_col_offset), reverse=True
    ):
        last_base = node.bases[-1]
        assert last_base.end_lineno is not None
        assert last_base.end_col_offset is not None
        line = lines[last_base.end_lineno - 1].encode()
        base = f', {BASE_CLASS}["{FILTERS_ALIAS}.{node.name}{FILTER_SUFFIX}"]'.encode()
        lines[last_base.end_lineno - 1] = (
            line[: last_base.end_col_offset] + base + line[last_base.end_col_offset :]
        ).decode()
    imports = [node for node in module.body if isinstance(node, ast.Import | ast.ImportFrom)]
    last_import = max((node.end_lineno or node.lineno for node in imports), default=0)  # models follow imports
    stub_module, _, stub_name = _stub_module(module_name).rpartition(".")
    lines.insert(
        last_import,
        f"from typing import TYPE_CHECKING\n\nfrom {BASE_CLASS_SOURCE} import {BASE_CLASS}\n\n"
        f"{_FILTERS_IMPORT_COMMENT}"
        f"if TYPE_CHECKING:\n    from {stub_module} import {stub_name} as {FILTERS_ALIAS}  # noqa: F401\n",
    )
    module_file.write_text("".join(lines))
