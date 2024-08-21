from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.constants import UNIVERSAL_BLOCK_LOGS_PATH


@pytest.fixture()
def alternate_chain_spec(block_log) -> tt.AlternateChainSpecs:
    return tt.AlternateChainSpecs.parse_file(block_log.path.parent / tt.AlternateChainSpecs.FILENAME)


@pytest.fixture()
def block_log() -> tt.BlockLog:
    return tt.BlockLog(path=UNIVERSAL_BLOCK_LOGS_PATH / "block_log_single_sign" / "block_log")
