from __future__ import annotations

import hashlib
import mmap
import shutil
import subprocess
from typing import TYPE_CHECKING, Final

import pytest

import test_tools as tt

if TYPE_CHECKING:
    from pathlib import Path

STARTUP_REFUSED_EXIT_CODE: Final[int] = 78

# The record of the account index's layout, a named object in shared_memory.bin. Boost.Interprocess stores a named
# object's value, then the length of its name (uint16), then the name with its terminating zero. The value is
# {uint64 size, uint64 type name hash}. All numbers are little-endian on x86-64.
LAYOUT_RECORD_NAME: Final[bytes] = b"hive::chain::account_object::layout"
LAYOUT_RECORD_SIZE: Final[int] = 16
NAME_LENGTH_SIZE: Final[int] = 2


def excerpt(output: str) -> str:
    """The lines worth seeing first when a check fails, followed by the end of the output."""
    keywords = ("layout", "refused", "error", "exception", "replay", "snapshot")
    relevant = [line for line in output.splitlines() if any(word in line.lower() for word in keywords)]
    return "\n".join(relevant[:40]) + "\n...\n" + output[-1500:]


def run_hived(data_dir: Path, *args: str) -> tuple[int, str]:
    try:
        hived = subprocess.run(
            [
                tt.paths_to_executables.get_path_of("hived"),
                "--data-dir",
                str(data_dir),
                "--shared-file-size",
                "128M",
                "--exit-before-sync",
                *args,
            ],
            cwd=data_dir.parent,
            capture_output=True,
            text=True,
            errors="replace",
            timeout=120,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        stderr = error.stderr.decode(errors="replace") if isinstance(error.stderr, bytes) else (error.stderr or "")
        pytest.fail(f"hived did not exit within {error.timeout} s:\n{excerpt(stderr)}")
    return hived.returncode, hived.stdout + hived.stderr


def enlarge_recorded_account_index_size(state_file: Path) -> int:
    """Changes the size in the account index's layout record, as a build with a larger index leaves it; returns it."""
    stored_name = LAYOUT_RECORD_NAME + b"\0"
    with state_file.open("r+b") as file, mmap.mmap(file.fileno(), 0) as state:
        position = state.find(stored_name)
        assert position >= NAME_LENGTH_SIZE + LAYOUT_RECORD_SIZE, f"no {stored_name!r} in {state_file}"
        assert state.find(stored_name, position + 1) == -1, f"{stored_name!r} is stored twice"
        name_length = int.from_bytes(state[position - NAME_LENGTH_SIZE : position], "little")
        assert name_length == len(LAYOUT_RECORD_NAME), f"{name_length} stored as the name length: unexpected layout"
        size_offset = position - NAME_LENGTH_SIZE - LAYOUT_RECORD_SIZE
        size = int.from_bytes(state[size_offset : size_offset + 8], "little")
        assert 0 < size < 1 << 20, f"{size} read as the size of the account index: the record is not where expected"
        enlarged = size + 8
        state[size_offset : size_offset + 8] = enlarged.to_bytes(8, "little")
        state.flush()
    return enlarged


def file_hash(path: Path) -> str:
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


@pytest.mark.testnet
def test_hived_refuses_state_with_another_layout_with_exit_code_78() -> None:
    data_dir = tt.context.get_current_directory() / "state_layout_refusal"
    shutil.rmtree(data_dir, ignore_errors=True)
    data_dir.mkdir(parents=True)
    state_file = data_dir / "blockchain" / "shared_memory.bin"

    exit_code, output = run_hived(data_dir)
    assert exit_code == 0, excerpt(output)
    assert state_file.exists(), excerpt(output)
    # the state opens as it is; the first open after its creation also clears a flag in it, which later opens leave
    exit_code, output = run_hived(data_dir)
    assert exit_code == 0, excerpt(output)

    recorded_size = enlarge_recorded_account_index_size(state_file)
    state_before = file_hash(state_file)

    # refused on every start, without touching the state, until it is rebuilt
    for _ in range(2):
        exit_code, output = run_hived(data_dir)
        assert exit_code == STARTUP_REFUSED_EXIT_CODE, excerpt(output)
        for expected in (
            f"The layout of hive::chain::account_object in `shared_memory_file` {{size: {recorded_size},",
            "differs from the one of the running build",
            "Start hived with --load-snapshot=<snapshot> to replace it with a snapshot",
            "or with --force-replay to rebuild it from the block log.",
            f"Exiting with code {STARTUP_REFUSED_EXIT_CODE}.",
        ):
            assert expected in output, f"'{expected}' not logged in:\n{excerpt(output)}"
        assert file_hash(state_file) == state_before
