from __future__ import annotations

import json
import os
import shutil
import subprocess
from typing import Final

import pytest

import test_tools as tt

# A locale that no system has installed.
UNINSTALLED_LOCALE: Final[str] = "xx_XX.UTF-8"

# A single category is enough to make std::locale("") throw; LC_ALL is emptied so that it cannot hide it
# (the CI image sets LC_ALL globally).
UNINSTALLED_LC_TIME: Final[dict[str, str]] = {"LC_ALL": "", "LANG": "C", "LC_TIME": UNINSTALLED_LOCALE}
UNINSTALLED_LC_ALL: Final[dict[str, str]] = {"LC_ALL": UNINSTALLED_LOCALE}

# What fc and hived print when a path conversion or the logging config hits the locale problem.
LOCALE_FAILURES: Final[tuple[str, ...]] = ("_S_create_c_locale", "codecvt", "Error parsing logging config")


def excerpt(output: str) -> str:
    """The lines worth seeing first when a check fails, followed by the end of the output."""
    keywords = ("snapshot", "error", "exception", "assert", "codecvt", "locale")
    relevant = [line for line in output.splitlines() if any(word in line.lower() for word in keywords)]
    return "\n".join(relevant[:40]) + "\n...\n" + output[-1500:]


def assert_no_locale_failure(output: str) -> None:
    for failure in LOCALE_FAILURES:
        assert failure not in output, f"found '{failure}' in:\n{excerpt(output)}"


def assert_reports_replaced_variable(output: str, variable: str) -> None:
    expected = f"{variable}={UNINSTALLED_LOCALE}"
    assert expected in output, f"{expected} not reported in:\n{excerpt(output)}"


def run_block_log_util(node: tt.InitNode, environment: dict[str, str], *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            tt.paths_to_executables.get_path_of("block_log_util"),
            "--get-head-block-number",
            "--block-log",  # the directory: a block log with no irreversible block yet has an empty part file
            str(node.block_log.path),
            *args,
        ],
        cwd=node.directory,  # block_log_util writes ./block_log_util.log
        env={**os.environ, **environment},
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )


@pytest.mark.testnet
def test_programs_work_when_environment_names_an_uninstalled_locale() -> None:
    node = tt.InitNode()
    node.run(environment_variables=UNINSTALLED_LC_TIME)
    node.wait_number_of_blocks(3)
    node.close()

    node_log = (node.directory / "stderr.log").read_text(errors="replace")
    assert_no_locale_failure(node_log)
    assert_reports_replaced_variable(node_log, "LC_TIME")

    for environment, variable in ((UNINSTALLED_LC_TIME, "LC_TIME"), (UNINSTALLED_LC_ALL, "LC_ALL")):
        result = run_block_log_util(node, environment)
        output = f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        assert result.returncode == 0, f"{environment}: {output}"
        assert_no_locale_failure(output)
        assert_reports_replaced_variable(result.stderr, variable)
        # The block log holds only irreversible blocks, so after a few blocks the head may still be 0.
        assert result.stdout.strip().isdigit(), f"{environment}: {output}"

    # With --json, block_log_util writes errors and most results as JSON to stderr, so the warning must stay out.
    result = run_block_log_util(node, UNINSTALLED_LC_ALL, "--json")
    assert result.returncode == 0, f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    try:
        json.loads(result.stderr)
    except json.JSONDecodeError as error:
        pytest.fail(f"stderr of block_log_util --json is not JSON ({error}):\n{result.stderr}")


@pytest.mark.testnet
def test_hived_handles_non_ascii_paths_in_c_locale() -> None:
    # The C locale itself is valid, so this depends on fc converting paths without the locale.
    c_locale = dict.fromkeys({"LANG", "LC_ALL", *(name for name in os.environ if name.startswith("LC_"))}, "")
    data_dir = tt.context.get_current_directory() / "za\u017c\u00f3\u0142\u0107"
    shutil.rmtree(data_dir, ignore_errors=True)  # an existing snapshot directory would make the dump fail

    try:
        hived = subprocess.run(
            [
                tt.paths_to_executables.get_path_of("hived"),
                "--data-dir",
                str(data_dir),
                "--shared-file-size",
                "128M",
                "--plugin",  # --dump-snapshot needs the state_snapshot plugin
                "state_snapshot",
                "--dump-snapshot",
                "snapshot",
                "--exit-before-sync",
            ],
            cwd=tt.context.get_current_directory(),
            env={**os.environ, **c_locale},
            capture_output=True,
            text=True,
            errors="replace",
            timeout=120,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        stderr = error.stderr.decode(errors="replace") if isinstance(error.stderr, bytes) else (error.stderr or "")
        pytest.fail(f"hived did not exit within {error.timeout} s:\n{excerpt(stderr)}")

    output = hived.stdout + hived.stderr
    assert hived.returncode == 0, excerpt(output)
    assert_no_locale_failure(output)
    # A failed snapshot dump is only logged, it does not change the exit code.
    assert "Snapshot generation finished" in output, excerpt(output)
