# private-test-exempt: asserts the deprecated hiveio_api._utilities alias is the public hiveio_api_client.lazy_import object
"""Checks of the deprecated `hiveio_api._utilities.smart_lazy_import`, imported by already published packages."""

from __future__ import annotations

import pytest

from tests.generated_package import require_generated_package

require_generated_package()


@pytest.mark.parametrize("name", ["aggregate_same_import", "lazy_module_factory", "smart_lazy_getattr"])
def test_deprecated_lazy_imports_are_the_objects_of_hiveio_api_client(name: str) -> None:
    # ARRANGE
    import hiveio_api._utilities.smart_lazy_import as deprecated
    import hiveio_api_client.lazy_import

    # ACT & ASSERT
    assert getattr(deprecated, name) is getattr(hiveio_api_client.lazy_import, name)
    assert name in deprecated.__all__
