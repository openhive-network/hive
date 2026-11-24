from __future__ import annotations

import hashlib
import mmap
import re
import shutil
import signal
import subprocess
import time
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
LAYOUT_RECORD_NAMES: Final[re.Pattern[bytes]] = re.compile(rb"hive::[\w:]+::layout\0")

OPEN_ERROR_MESSAGE: Final[str] = "Error opening database or block log"

# the plugins that add indexes to the state, besides those hived always adds
INDEX_PLUGINS: Final[tuple[str, ...]] = (
    "account_by_key",
    "account_history_rocksdb",
    "block_log_info",
    "market_history",
    "metadata",
    "reputation",
    "transaction_status",
)

INCOMPLETE_STATE_MESSAGES: Final[tuple[str, ...]] = (
    "The state in `shared_memory_file` was not completely created: its first open did not complete (it was interrupted"
    " or failed). A replay (or a snapshot load) is needed.",
    "The lock of `shared_memory_file` was left held by a process that was killed while it changed the state, so the"
    " state may be inconsistent. A replay (or a snapshot load) is needed.",
)


def assert_no_open_error(output: str) -> None:
    """Checks that a refused start does not also log the generic open error, which adds nothing to the refusal."""
    assert OPEN_ERROR_MESSAGE not in output, f"'{OPEN_ERROR_MESSAGE}' logged on a refusal:\n{excerpt(output)}"


def assert_logged_once(output: str, message: str) -> None:
    """Checks that the refusal `message` is reported once: fc logs an exception's message, then its {"what": ...}."""
    count = output.count(f'"what":"{message}')
    assert count == 1, f"'{message}' reported {count} times in:\n{excerpt(output)}"


def excerpt(output: str) -> str:
    """The lines worth seeing first when a check fails, followed by the end of the output."""
    keywords = ("layout", "refused", "error", "exception", "replay", "snapshot")
    relevant = [line for line in output.splitlines() if any(word in line.lower() for word in keywords)]
    return "\n".join(relevant[:40]) + "\n...\n" + output[-1500:]


def hived_command(data_dir: Path, *args: str, shared_file_size: str = "128M") -> list[str]:
    return [
        str(tt.paths_to_executables.get_path_of("hived")),
        "--data-dir",
        str(data_dir),
        "--shared-file-size",
        shared_file_size,
        "--exit-before-sync",
        *args,
    ]


def run_hived(data_dir: Path, *args: str, shared_file_size: str = "128M") -> tuple[int, str]:
    try:
        hived = subprocess.run(
            hived_command(data_dir, *args, shared_file_size=shared_file_size),
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


def recorded_layouts(state_file: Path) -> set[str]:
    """The names of the containers whose layout hived recorded in `state_file`."""
    with state_file.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as state:
        names: set[str] = set()
        for record in LAYOUT_RECORD_NAMES.finditer(state):
            name = record.group()[:-1]
            name_length = int.from_bytes(state[record.start() - NAME_LENGTH_SIZE : record.start()], "little")
            assert name_length == len(name), f"{name_length} stored as the length of {name!r}: unexpected layout"
            names.add(name.decode())
    return names


def shared_memory_file_util_path() -> Path:
    hived: Path = tt.paths_to_executables.get_path_of("hived")
    # beside hived where binaries are installed, in programs/util of a build directory
    candidates = (hived.parent / "shared_memory_file_util", hived.parent.parent / "util" / "shared_memory_file_util")
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    pytest.fail(f"shared_memory_file_util not found at any of {[str(candidate) for candidate in candidates]}")


def run_shared_memory_file_util(data_dir: Path, *args: str) -> tuple[int, str, str]:
    util = subprocess.run(
        [str(shared_memory_file_util_path()), "--input", str(data_dir / "blockchain"), *args],
        capture_output=True,
        text=True,
        errors="replace",
        timeout=120,
        check=False,
    )
    return util.returncode, util.stdout, util.stderr


def kill_hived_during_first_open(data_dir: Path, delay: float) -> None:
    """Starts hived on a fresh data directory and kills it with SIGKILL `delay` seconds after it creates its state."""
    state_file = data_dir / "blockchain" / "shared_memory.bin"
    hived = subprocess.Popen(
        hived_command(data_dir), cwd=data_dir.parent, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    try:
        deadline = time.monotonic() + 60
        # busy, because the whole open takes a few tens of milliseconds
        while not state_file.exists():
            assert hived.poll() is None, f"hived exited with {hived.returncode} before it created {state_file}"
            assert time.monotonic() < deadline, f"hived did not create {state_file} within 60 s"
        time.sleep(delay)
    finally:
        hived.send_signal(signal.SIGKILL)
        hived.wait()


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
        assert_logged_once(output, "The layout of hive::chain::account_object in `shared_memory_file`")
        assert_no_open_error(output)
        assert file_hash(state_file) == state_before


@pytest.mark.testnet
def test_shared_memory_file_util_reads_the_indexes_of_state_hived_created() -> None:
    data_dir = tt.context.get_current_directory() / "state_read_by_util"
    shutil.rmtree(data_dir, ignore_errors=True)
    data_dir.mkdir(parents=True)

    exit_code, output = run_hived(data_dir, *(f"--plugin={plugin}" for plugin in INDEX_PLUGINS))
    assert exit_code == 0, excerpt(output)

    # the util checks the layout of each index hived recorded against its own, so both have to compute it alike
    exit_code, stdout, stderr = run_shared_memory_file_util(data_dir, "--list-indices")
    assert exit_code == 0, f"stdout:\n{stdout}\nstderr:\n{excerpt(stderr)}"
    detected = re.match(r"Detected (\d+) indices:", stdout)
    assert detected, stdout
    assert "account_object" in stdout, stdout
    # every index hived added, each of which it recorded the layout of, is one the util knows
    layouts = recorded_layouts(data_dir / "blockchain" / "shared_memory.bin")
    assert "hive::chain::state_stamp_data_object::layout" in layouts, layouts
    assert int(detected.group(1)) == len(layouts), f"the util detected {detected.group(1)} of:\n{sorted(layouts)}"

    dump_dir = data_dir / "dump"
    exit_code, stdout, stderr = run_shared_memory_file_util(data_dir, "--dump-indices", "--output", str(dump_dir))
    assert exit_code == 0, f"stdout:\n{stdout}\nstderr:\n{excerpt(stderr)}"
    # the genesis accounts, initminer among them
    account_dumps = list((dump_dir / "account_object").glob("*.log"))
    assert account_dumps, f"no dump of account_object in {sorted(path.name for path in dump_dir.iterdir())}"
    assert any("initminer" in dump.read_text(errors="replace") for dump in account_dumps), account_dumps


def assert_refused_as_incomplete(exit_code: int, output: str, message: str | None = None) -> None:
    """Checks that hived refused the state with code 78 as one whose first open did not complete."""
    assert exit_code == STARTUP_REFUSED_EXIT_CODE, excerpt(output)
    expected_messages = (message,) if message is not None else INCOMPLETE_STATE_MESSAGES
    logged = [expected for expected in expected_messages if expected in output]
    assert logged, f"none of {expected_messages} logged in:\n{excerpt(output)}"
    assert_logged_once(output, logged[0])
    assert_no_open_error(output)
    for expected in (
        "or with --force-replay to rebuild it from the block log.",
        f"Exiting with code {STARTUP_REFUSED_EXIT_CODE}.",
    ):
        assert expected in output, f"'{expected}' not logged in:\n{excerpt(output)}"


def assert_refused_without_growing(data_dir: Path) -> None:
    """Restarts hived with a larger shared file size: it is refused the same way and the state file is left as it is."""
    state_file = data_dir / "blockchain" / "shared_memory.bin"
    state_before = file_hash(state_file)
    size_before = state_file.stat().st_size
    for shared_file_size in ("128M", "256M"):
        exit_code, output = run_hived(data_dir, shared_file_size=shared_file_size)
        assert_refused_as_incomplete(exit_code, output)
        assert state_file.stat().st_size == size_before
        assert file_hash(state_file) == state_before


@pytest.mark.testnet
def test_hived_refuses_state_of_a_failed_first_open_with_exit_code_78() -> None:
    data_dir = tt.context.get_current_directory() / "failed_first_open"
    shutil.rmtree(data_dir, ignore_errors=True)
    blockchain_dir = data_dir / "blockchain"
    blockchain_dir.mkdir(parents=True)
    # a file where the comment archive's RocksDB directory belongs makes the first open fail after creating the state
    comment_archive = blockchain_dir / "comments-rocksdb-storage"
    comment_archive.write_bytes(b"")

    exit_code, output = run_hived(data_dir)
    assert exit_code not in (0, STARTUP_REFUSED_EXIT_CODE), excerpt(output)
    assert (blockchain_dir / "shared_memory.bin").exists(), excerpt(output)

    # refused once the cause is gone too: the state is still incomplete
    comment_archive.unlink()
    exit_code, output = run_hived(data_dir)
    assert_refused_as_incomplete(exit_code, output, INCOMPLETE_STATE_MESSAGES[0])
    assert not (comment_archive / "CURRENT").exists(), "the comment archive was opened before the state was refused"
    assert_refused_without_growing(data_dir)


@pytest.mark.testnet
def test_hived_refuses_state_of_an_interrupted_first_open_with_exit_code_78() -> None:
    root = tt.context.get_current_directory() / "interrupted_first_open"
    shutil.rmtree(root, ignore_errors=True)
    exit_codes: list[int] = []
    refused_data_dir: Path | None = None
    # from right after the state file appears, through the construction of the indexes, to past the end of the open
    for delay in (0.0, 0.001, 0.003, 0.01, 0.02, 0.04, 0.08):
        data_dir = root / f"delay_{delay}"
        data_dir.mkdir(parents=True)
        kill_hived_during_first_open(data_dir, delay)

        exit_code, output = run_hived(data_dir)
        tt.logger.info(f"Killed {delay * 1000} ms after the state was created, restart exited with {exit_code}")
        exit_codes.append(exit_code)
        assert exit_code in (0, STARTUP_REFUSED_EXIT_CODE), excerpt(output)
        if exit_code == STARTUP_REFUSED_EXIT_CODE:
            refused_data_dir = data_dir
            assert_refused_as_incomplete(exit_code, output)

    assert refused_data_dir is not None, f"no kill interrupted the first open; restarts exited with {exit_codes}"
    # refused again, also by a start that would grow the state file, which is left as it is
    assert_refused_without_growing(refused_data_dir)
