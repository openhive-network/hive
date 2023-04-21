from typing import Any
import pytest

from shared_tools.cpp_helper_functions import cpp_prepare


@pytest.fixture(scope="package")
def cpp() -> Any:
    cpp_prepare()

    import cpp_interface

    return cpp_interface
