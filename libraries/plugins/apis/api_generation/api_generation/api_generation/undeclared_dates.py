"""
Check of openapi.json - dates are always declared (`format: date-time`), never left as plain strings.

Dates of public models are the user-friendly `hiveio_api.HiveDateTime`. A string in the time format of Hive (e.g.
`2016-03-24T16:05:00`) in an example of a response, in a place whose schema is not a date, means the schema lacks
`format: date-time` - generation fails, so the definition gets fixed instead of the API returning a plain string where
all other dates are `HiveDateTime`.
"""

from __future__ import annotations

import json
import re
from collections.abc import Mapping
from typing import TYPE_CHECKING, Final

from hiveio_api.fields.hive_constants import HIVE_TIME_PATTERN

if TYPE_CHECKING:
    from collections.abc import Iterable
    from pathlib import Path

DATE_FORMAT: Final[str] = "date-time"
"""Format of strings holding dates in openapi.json."""

_DATE: Final = re.compile(HIVE_TIME_PATTERN)
_REFERENCE_PREFIX: Final[str] = "#/components/schemas/"
_MAX_REFERENCE_CHAIN: Final[int] = 64


class UndeclaredDateError(ValueError):
    """An example of a response holds a date where openapi.json lacks `format: date-time`."""


def check_no_undeclared_dates(openapi_json_path: Path, apis: Iterable[str]) -> None:
    """
    Raise when an example of a response of any of the APIs holds a date in a place not declared as a date.

    Args:
        openapi_json_path: openapi.json - read once for all the APIs.
        apis: Names of the APIs to check (e.g. `database_api`).

    Raises:
        UndeclaredDateError: When a date is found in a string without `format: date-time` (or without any schema) - of
            the first API with such dates.
    """
    definition = json.loads(openapi_json_path.read_text(encoding="utf-8"))
    for api in apis:
        undeclared = find_undeclared_dates(definition, api)
        if undeclared:
            raise UndeclaredDateError(
                f"Undeclared date detected in responses of `{api}`: {', '.join(undeclared)} - fix openapi.json: "
                'declare the date with `"format": "date-time"`!'
            )


def find_undeclared_dates(definition: Mapping[str, object], api: str) -> list[str]:
    """Places (`<endpoint>: <path in the response>`) of dates in examples of responses not declared as dates."""
    schemas = _Schemas(_mapping(_mapping(definition.get("components")).get("schemas")))
    places: set[str] = set()
    for endpoint, item in _mapping(definition.get("paths")).items():
        if not endpoint.startswith(f"{api}."):
            continue
        response = _mapping(_mapping(_mapping(_mapping(item).get("post")).get("responses")).get("200"))
        content = _mapping(_mapping(response.get("content")).get("application/json"))
        examples = [content["example"]] if "example" in content else []
        examples += [_mapping(example).get("value") for example in _mapping(content.get("examples")).values()]
        for example in examples:
            places.update(f"{endpoint}: {path}" for path in schemas.undeclared(content.get("schema"), example, "result"))
    return sorted(places)


class _Schemas:
    """Schemas of openapi.json (`components/schemas`), walked together with examples."""

    def __init__(self, components: Mapping[str, object]) -> None:
        self.__components = components

    def undeclared(self, schema: object, example: object, path: str) -> list[str]:
        """Paths of dates in the example in places the schema (`None` - no schema) does not declare as dates."""
        alternatives = [] if schema is None else self.__alternatives(schema)
        if isinstance(example, str):
            declared = any(alternative.get("format") == DATE_FORMAT for alternative in alternatives)
            return [path] if _DATE.match(example) and not declared else []
        if isinstance(example, Mapping):
            return [
                place
                for key, value in example.items()
                for place in self.undeclared(_member(alternatives, key), value, f"{path}.{key}")
            ]
        if isinstance(example, list):
            return [
                place
                for index, item in enumerate(example)
                for place in self.undeclared(_item(alternatives, index), item, f"{path}[]")
            ]
        return []

    def __alternatives(self, schema: object) -> list[Mapping[str, object]]:
        """The schema with references resolved - each member of `oneOf` / `anyOf`, members of `allOf` merged."""
        node = self.__resolve(schema)
        for keyword in ("oneOf", "anyOf"):
            members = node.get(keyword)
            if isinstance(members, list):
                return [alternative for member in members for alternative in self.__alternatives(member)]
        members = node.get("allOf")
        if not isinstance(members, list):
            return [node]
        merged: dict[str, object] = {key: value for key, value in node.items() if key != "allOf"}
        properties: dict[str, object] = {}
        for member in members:
            for alternative in self.__alternatives(member):
                properties.update(_mapping(alternative.get("properties")))
                merged.update({key: value for key, value in alternative.items() if key != "properties"})
        merged["properties"] = properties | dict(_mapping(node.get("properties")))
        return [merged]

    def __resolve(self, schema: object) -> Mapping[str, object]:
        node = _mapping(schema)
        for _ in range(_MAX_REFERENCE_CHAIN):
            reference = node.get("$ref")
            if not isinstance(reference, str):
                break
            node = _mapping(self.__components.get(reference.removeprefix(_REFERENCE_PREFIX)))
        return node


def _member(alternatives: list[Mapping[str, object]], key: str) -> object:
    """Schema of the member of objects - `None` when no alternative describes it."""
    members: list[object] = []
    for alternative in alternatives:
        properties = _mapping(alternative.get("properties"))
        additional = alternative.get("additionalProperties")
        if key in properties:
            members.append(properties[key])
        elif isinstance(additional, Mapping):
            members.append(additional)
    return _any_of(members)


def _item(alternatives: list[Mapping[str, object]], index: int) -> object:
    """Schema of the item of arrays (by its position in tuples) - `None` when no alternative describes it."""
    items: list[object] = []
    for alternative in alternatives:
        prefix = alternative.get("prefixItems")
        item = alternative.get("items")
        if isinstance(prefix, list) and index < len(prefix):
            items.append(prefix[index])
        elif isinstance(item, Mapping):
            items.append(item)
    return _any_of(items)


def _any_of(schemas: list[object]) -> object:
    if not schemas:
        return None
    return schemas[0] if len(schemas) == 1 else {"anyOf": schemas}


def _mapping(value: object) -> Mapping[str, object]:
    return value if isinstance(value, Mapping) else {}
