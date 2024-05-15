from __future__ import annotations

import pytest

import test_tools as tt


@pytest.fixture()
def node() -> tt.InitNode:
    node = tt.InitNode()
    node.run()
    return node


@pytest.fixture()
def wallet(node) -> tt.OldWallet:
    return tt.OldWallet(attach_to=node)
