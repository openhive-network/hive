from __future__ import annotations
import os
from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture()
def alternate_chain_spec(block_log_single_sign) -> tt.AlternateChainSpecs:
    return tt.AlternateChainSpecs.parse_file(block_log_single_sign.path / tt.AlternateChainSpecs.FILENAME)


@pytest.fixture()
def block_log_single_sign() -> tt.BlockLog:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    block_log_directory = Path(destination_variable) / "single_sign_universal_block_log"
    return tt.BlockLog(block_log_directory, "auto")
