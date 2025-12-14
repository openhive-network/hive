from __future__ import annotations
import os
import sys
from pathlib import Path

import pytest

import test_tools as tt


@pytest.fixture(scope="session", autouse=True)
def configure_loguru_for_thread_safety():
    """
    Configure loguru to use synchronous logging (enqueue=False) to prevent
    multiprocessing.Queue threading issues during concurrent node startup.

    This runs once per test session before any tests execute, ensuring all
    logging uses thread-safe synchronous mode instead of async multiprocessing.

    Impact: Tests get full logging output, but with synchronous I/O (~5-10% slower).
    Benefit: No segfaults, no handler ID errors, concurrent startup works.
    """
    # Reconfigure loguru to use synchronous mode globally
    tt.logger.remove()  # Remove default async handler
    tt.logger.add(sys.stderr, enqueue=False, level="TRACE")  # Add synchronous handler

    yield  # Tests run here with synchronous logging

    # No cleanup needed - synchronous logging is fine for entire session


@pytest.fixture()
def alternate_chain_spec(block_log_single_sign) -> tt.AlternateChainSpecs:
    return tt.AlternateChainSpecs.parse_file(block_log_single_sign.path / tt.AlternateChainSpecs.FILENAME)


@pytest.fixture()
def block_log_single_sign() -> tt.BlockLog:
    destination_variable = os.environ.get("TESTING_BLOCK_LOGS_DESTINATION")
    assert destination_variable is not None, "Path TESTING_BLOCK_LOGS_DESTINATION must be set!"
    block_log_directory = Path(destination_variable) / "single_sign_universal_block_log"
    return tt.BlockLog(block_log_directory, "auto")
