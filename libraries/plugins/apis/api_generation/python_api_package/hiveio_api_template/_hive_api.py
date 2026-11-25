"""Base class of the generated API clients of hiveio_api."""

from __future__ import annotations

from typing import TYPE_CHECKING

from hiveio_api._schema_validation import compile_validation_mask
from hiveio_api_client import AbstractAsyncApi

if TYPE_CHECKING:
    from collections.abc import Mapping

__all__ = ["HiveAsyncApi"]


class HiveAsyncApi(AbstractAsyncApi):
    """
    Client of an API of hived with validation models (`__validation_module__`).

    Validation masks of its endpoints (`set_validation_mask`) are compiled when they are set - to the validation model
    of the endpoint reduced to the fields of the mask, used by `hiveio_api.validate_json` / `validate_schema`.
    """

    def _compile_validation_mask(self, endpoint_name: str, mask: Mapping[str, object]) -> object:
        return compile_validation_mask(type(self), endpoint_name, mask)
