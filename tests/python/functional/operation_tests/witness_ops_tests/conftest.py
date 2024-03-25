from __future__ import annotations

import pytest

from python.functional.operation_tests.conftest import WitnessAccount


@pytest.fixture()
def create_account_object() -> type[WitnessAccount]:
    return WitnessAccount
