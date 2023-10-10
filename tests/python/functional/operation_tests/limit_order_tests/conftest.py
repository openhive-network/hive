from __future__ import annotations

import pytest

from ..conftest import LimitOrderAccount


@pytest.fixture()
def create_account_object():
    return LimitOrderAccount
