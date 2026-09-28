from __future__ import annotations

import contextlib
import os
import select
import subprocess
import time
from typing import TYPE_CHECKING

import test_tools as tt

if TYPE_CHECKING:
    from pathlib import Path

# cli_wallet writes both command results and exception details to stdout (fc::rpc::cli),
# so merging stderr into stdout is enough to observe everything an instance reports.
_SESSION_TIMEOUT_SECONDS = 60
_READY_TIMEOUT_SECONDS = 30
_REFUSAL_TIMEOUT_SECONDS = 30
_TERMINATE_TIMEOUT_SECONDS = 30

# Substring of the FC_ASSERT message raised by save_wallet_file() when the wallet file
# changed on disk since it was loaded (issue #288).
_REFUSAL_MARKER = "modified by another process"

# normalize_brain_key is an offline, side-effect-free command that echoes its argument back
# upper-cased; we use it as a readiness probe to know an instance has started and loaded the
# wallet file (and so captured its baseline) before we let another instance touch the file.
_READY_TOKEN = "walletready"
_READY_MARKER = _READY_TOKEN.upper()


def _cli_wallet_path() -> str:
    return tt.paths_to_executables.get_path_of("cli_wallet")


def _run_session(wallet_file: Path, commands: str) -> subprocess.CompletedProcess[str]:
    """Feed a batch of commands to an offline cli_wallet, then let it exit on stdin EOF."""
    return subprocess.run(
        [_cli_wallet_path(), "--offline", "--wallet-file", str(wallet_file)],
        input=commands,
        capture_output=True,
        text=True,
        timeout=_SESSION_TIMEOUT_SECONDS,
    )


def _read_until(process: subprocess.Popen[str], marker: str, timeout: float) -> str:
    """Accumulate a process' stdout until it contains marker or the timeout elapses."""
    deadline = time.monotonic() + timeout
    output = ""
    assert process.stdout is not None
    while time.monotonic() < deadline:
        ready, _, _ = select.select([process.stdout], [], [], 0.2)
        if not ready:
            continue
        chunk = os.read(process.stdout.fileno(), 4096).decode("utf-8", "replace")
        if chunk == "":  # EOF
            break
        output += chunk
        if marker in output:
            break
    return output


def _terminate(process: subprocess.Popen[str]) -> None:
    if process.stdin is not None and not process.stdin.closed:
        with contextlib.suppress(OSError):
            process.stdin.close()
    process.terminate()
    try:
        process.wait(timeout=_TERMINATE_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        process.kill()


def test_concurrent_save_is_refused_after_external_modification(tmp_path: Path) -> None:
    """A cli_wallet instance must refuse to overwrite a wallet file that another instance
    changed underneath it, instead of silently dropping the other instance's keys (#288)."""
    wallet_file = tmp_path / "wallet.json"
    password = "pass"
    first_key = str(tt.Account("alice").private_key)
    second_key = str(tt.Account("bob").private_key)
    third_key = str(tt.Account("carol").private_key)

    # Create the shared wallet file with a single key.
    _run_session(wallet_file, f"set_password {password}\nunlock {password}\nimport_key {first_key}\n")
    assert wallet_file.exists(), "initial wallet file was not created"

    # Instance A opens the wallet and captures its on-disk baseline at load time.
    instance_a = subprocess.Popen(
        [_cli_wallet_path(), "--offline", "--wallet-file", str(wallet_file)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    try:
        assert instance_a.stdin is not None
        # The reply to normalize_brain_key confirms A is up and has loaded the wallet file.
        instance_a.stdin.write(f"unlock {password}\nnormalize_brain_key {_READY_TOKEN}\n")
        instance_a.stdin.flush()
        ready_output = _read_until(instance_a, _READY_MARKER, _READY_TIMEOUT_SECONDS)
        assert _READY_MARKER in ready_output, "instance A did not finish starting up"

        # A second instance imports another key and saves, changing the file on disk.
        baseline = wallet_file.read_text()
        _run_session(wallet_file, f"unlock {password}\nimport_key {second_key}\n")
        changed = wallet_file.read_text()
        assert changed != baseline, "second instance should have modified the wallet file"

        # A now tries to save: it must be refused and must not clobber the second instance's data.
        instance_a.stdin.write(f"import_key {third_key}\n")
        instance_a.stdin.flush()
        refusal_output = _read_until(instance_a, _REFUSAL_MARKER, _REFUSAL_TIMEOUT_SECONDS)
        assert _REFUSAL_MARKER in refusal_output, f"instance A should have refused to save; got:\n{refusal_output}"
        assert wallet_file.read_text() == changed, "instance A must not overwrite the concurrently saved wallet"
    finally:
        _terminate(instance_a)


def test_repeated_saves_in_a_single_session_succeed(tmp_path: Path) -> None:
    """The concurrency guard must not produce false positives: an instance can keep saving
    its own changes, and they persist across reopen."""
    wallet_file = tmp_path / "wallet.json"
    password = "pass"
    first_key = str(tt.Account("alice").private_key)
    second_key = str(tt.Account("bob").private_key)

    first = _run_session(
        wallet_file,
        f"set_password {password}\nunlock {password}\nimport_key {first_key}\nimport_key {second_key}\n",
    )
    assert _REFUSAL_MARKER not in first.stdout, f"unexpected refusal on normal save:\n{first.stdout}"
    assert wallet_file.exists()

    reopened = _run_session(wallet_file, f"unlock {password}\nlist_keys\n")
    assert first_key in reopened.stdout, "first imported key did not persist"
    assert second_key in reopened.stdout, "second imported key did not persist"
