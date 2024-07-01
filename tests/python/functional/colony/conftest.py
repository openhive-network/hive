from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture()
def alternate_chain_spec(block_log):
    return tt.AlternateChainSpecs.parse_file(
        Path("/home/dev/workspace_ssh/tests_block_logs_clive/block_log_single_sign/") / tt.AlternateChainSpecs.FILENAME
    )


@pytest.fixture()
def block_log():
    protected_block_log_destination = Path(__file__).parent / "block_log"
    protected_block_log = tt.BlockLog(path=protected_block_log_destination / "block_log")
    return protected_block_log.copy_to(Path(tempfile.gettempdir()))
