from __future__ import annotations

import pytest

from python.functional.operation_tests.conftest import ProposalAccount


@pytest.fixture()
def create_account_object() -> ProposalAccount:
    return ProposalAccount
