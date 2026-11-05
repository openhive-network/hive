"""
Deprecated - use `hiveio_api_client.lazy_import` instead.

The lazy-import helpers moved to hiveio-api-client; this module only keeps the old import path of already published
packages (beekeepy, test-tools, clive) working. Its names are the objects of `hiveio_api_client.lazy_import`.
"""

from __future__ import annotations

from hiveio_api_client.lazy_import import (
    aggregate_same_import,
    lazy_module_factory,
    smart_lazy_getattr,
)

__all__ = ["aggregate_same_import", "lazy_module_factory", "smart_lazy_getattr"]
