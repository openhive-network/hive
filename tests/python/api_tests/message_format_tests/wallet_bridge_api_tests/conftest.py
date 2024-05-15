from __future__ import annotations

import pytest
from loguru import logger

import test_tools as tt


@pytest.fixture(autouse=True)
def _disable_logging() -> None:
    logger.disable("helpy")


def pytest_configure(config):
    config.addinivalue_line("markers", "enabled_plugins: Enabled plugins in node from `node` fixture")


@pytest.fixture()
def wallet(node: tt.InitNode) -> tt.Wallet:
    return tt.Wallet(attach_to=node)
