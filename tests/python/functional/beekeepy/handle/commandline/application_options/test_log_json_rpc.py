from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path

    from beekeepy._handle import Beekeeper


def check_log_json_rpc(log_json_rpc_path: Path) -> None:
    """Check if log-json-rpc directory has been created, and contains required files."""
    json_1_path = log_json_rpc_path / "1.json"
    json_1_pat_path = log_json_rpc_path / "1.json.pat"

    assert json_1_path.exists()
    assert json_1_pat_path.exists()


def test_log_json_rpc(beekeeper_not_started: Beekeeper) -> None:
    """Test will check command line flag --log-json-rpc."""
    # ARRANGE
    path_to_jsonrpc_logs = beekeeper_not_started.settings.working_directory
    beekeeper_not_started.config.log_json_rpc = path_to_jsonrpc_logs

    # ACT
    with beekeeper_not_started:
        beekeeper_not_started.api.beekeeper.get_info()

        # ASSERT
        check_log_json_rpc(path_to_jsonrpc_logs)
