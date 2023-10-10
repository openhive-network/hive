from __future__ import annotations

import pytest

from python.functional.operation_tests.conftest import LimitOrderAccount


@pytest.fixture()
def create_account_object():
    return LimitOrderAccount
