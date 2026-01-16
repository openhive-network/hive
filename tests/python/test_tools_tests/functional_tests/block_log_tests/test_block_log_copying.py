from __future__ import annotations

import shutil
from typing import TYPE_CHECKING

import pytest
import test_tools as tt

if TYPE_CHECKING:
    from pathlib import Path

    from test_tools.__private.block_log import BlockLog


@pytest.fixture
def destination_directory(tmp_path: Path) -> Path:
    """Block log files in tests are copied to this directory."""
    directory = tmp_path / "destination"
    directory.mkdir()
    return directory


@pytest.fixture
def source_directory(tmp_path: Path) -> Path:
    """Block log files in tests are created here and copied from this directory."""
    directory = tmp_path / "source"
    directory.mkdir()
    return directory


@pytest.fixture
def block_log_stub(source_directory: Path) -> list[BlockLog]:
    split_dir = source_directory / "split"
    split_dir.mkdir()
    (split_dir / "block_log_part.0001").touch()
    mono_dir = source_directory / "monolithic"
    mono_dir.mkdir()
    (mono_dir / "block_log").touch()
    return [tt.BlockLog(split_dir, "split"), tt.BlockLog(mono_dir, "monolithic")]


@pytest.fixture
def artifacts_stub(source_directory: Path) -> None:
    split_dir = source_directory / "split"
    (split_dir / "block_log_part.0001.artifacts").touch()
    mono_dir = source_directory / "monolithic"
    (mono_dir / "block_log.artifacts").touch()


def test_paths_in_copied_block_log(
    block_log_stub: list[BlockLog], artifacts_stub: Path, destination_directory: Path  # noqa: ARG001
) -> None:
    # split log:
    copied_block_log = block_log_stub[0].copy_to(destination_directory, artifacts="required")
    assert copied_block_log.path == destination_directory
    __assert_files_were_copied(block_log_stub[0], copied_block_log, require_artifacts=True)

    shutil.rmtree(destination_directory)
    destination_directory.mkdir()

    # mono log:
    copied_block_log = block_log_stub[1].copy_to(destination_directory, artifacts="required")
    assert copied_block_log.path == destination_directory
    __assert_files_were_copied(block_log_stub[1], copied_block_log, require_artifacts=True)


def test_copying_when_required_artifacts_exists(
    block_log_stub: list[BlockLog], artifacts_stub: Path, destination_directory: Path  # noqa: ARG001
) -> None:
    # split log:
    copied_block_log = block_log_stub[0].copy_to(destination_directory, artifacts="required")
    __assert_files_were_copied(block_log_stub[0], copied_block_log, require_artifacts=True)

    shutil.rmtree(destination_directory)
    destination_directory.mkdir()

    # mono log:
    copied_block_log = block_log_stub[1].copy_to(destination_directory, artifacts="required")
    __assert_files_were_copied(block_log_stub[1], copied_block_log, require_artifacts=True)


def test_copying_when_required_artifacts_are_missing(
    block_log_stub: list[BlockLog], destination_directory: Path
) -> None:
    # split log:
    with pytest.raises(tt.exceptions.MissingBlockLogArtifactsError):
        block_log_stub[0].copy_to(destination_directory, artifacts="required")

    assert __is_empty(destination_directory)  # When error occurs nothing should be copied.

    # mono log:
    with pytest.raises(tt.exceptions.MissingBlockLogArtifactsError):
        block_log_stub[1].copy_to(destination_directory, artifacts="required")

    assert __is_empty(destination_directory)  # When error occurs nothing should be copied.


def test_copying_when_optional_artifacts_exists(
    block_log_stub: list[BlockLog], artifacts_stub: Path, destination_directory: Path  # noqa: ARG001
) -> None:
    # split log:
    copied_block_log = block_log_stub[0].copy_to(destination_directory, artifacts="optional")
    __assert_files_were_copied(block_log_stub[0], copied_block_log, require_artifacts=True)

    shutil.rmtree(destination_directory)
    destination_directory.mkdir()

    # mono log:
    copied_block_log = block_log_stub[1].copy_to(destination_directory, artifacts="optional")
    __assert_files_were_copied(block_log_stub[1], copied_block_log, require_artifacts=True)


def test_copying_when_optional_artifacts_are_missing(
    block_log_stub: list[BlockLog], destination_directory: Path
) -> None:
    # split log:
    copied_block_log = block_log_stub[0].copy_to(destination_directory, artifacts="optional")
    __assert_files_were_copied(block_log_stub[0], copied_block_log, require_artifacts=False)

    shutil.rmtree(destination_directory)
    destination_directory.mkdir()

    # mono log:
    copied_block_log = block_log_stub[1].copy_to(destination_directory, artifacts="optional")
    __assert_files_were_copied(block_log_stub[1], copied_block_log, require_artifacts=False)


def test_copying_when_excluded_artifacts_exists(
    block_log_stub: list[BlockLog], artifacts_stub: Path, destination_directory: Path  # noqa: ARG001
) -> None:
    # split log:
    copied_block_log = block_log_stub[0].copy_to(destination_directory, artifacts="excluded")
    __assert_files_were_copied(block_log_stub[0], copied_block_log, require_artifacts=False)

    shutil.rmtree(destination_directory)
    destination_directory.mkdir()

    # mono log:
    copied_block_log = block_log_stub[1].copy_to(destination_directory, artifacts="excluded")
    __assert_files_were_copied(block_log_stub[1], copied_block_log, require_artifacts=False)


def test_copying_when_excluded_artifacts_are_missing(
    block_log_stub: list[BlockLog], destination_directory: Path
) -> None:
    # split log:
    copied_block_log = block_log_stub[0].copy_to(destination_directory, artifacts="excluded")
    __assert_files_were_copied(block_log_stub[0], copied_block_log, require_artifacts=False)

    shutil.rmtree(destination_directory)
    destination_directory.mkdir()

    # mono log:
    copied_block_log = block_log_stub[1].copy_to(destination_directory, artifacts="excluded")
    __assert_files_were_copied(block_log_stub[1], copied_block_log, require_artifacts=False)


def test_error_reporting_when_artifacts_have_unsupported_value(
    block_log_stub: list[BlockLog], destination_directory: Path
) -> None:
    # split log:
    with pytest.raises(ValueError):  # noqa: PT011
        block_log_stub[0].copy_to(destination_directory, artifacts="unsupported_value")  # type: ignore[arg-type]

    # mono log:
    with pytest.raises(ValueError):  # noqa: PT011
        block_log_stub[1].copy_to(destination_directory, artifacts="unsupported_value")  # type: ignore[arg-type]


def __is_empty(directory: Path) -> bool:
    assert directory.is_dir()
    return not any(directory.iterdir())


def __assert_files_were_copied(
    source_block_log: tt.BlockLog, block_log: tt.BlockLog, *, require_artifacts: bool, require_block_log: bool = True
) -> None:
    assert block_log.path.exists()

    # Make sure, that exactly required number of files are copied and nothing more.
    if require_block_log:
        assert len(block_log.block_files) == len(source_block_log.block_files)

    if require_artifacts:
        assert len(block_log.artifact_files) == len(source_block_log.artifact_files)
