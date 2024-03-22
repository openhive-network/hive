from __future__ import annotations

from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture()
def block_log():
    protected_block_log_destination = Path(
        "/home/dev/workspace_ssh/block_log_sources/tests_block_logs/block_log_open_sign/"
    )
    protected_block_log = tt.BlockLog(path=protected_block_log_destination / "block_log")
    return protected_block_log.copy_to(Path(__file__).parent)


@pytest.fixture()
def alternate_chain_spec(block_log):
    return tt.AlternateChainSpecs.parse_file(
        Path("/home/dev/workspace_ssh/block_log_sources/tests_block_logs/block_log_open_sign/")
        / tt.AlternateChainSpecs.FILENAME
    )
