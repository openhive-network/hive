from __future__ import annotations

from typing import TYPE_CHECKING

from clive.__private.core.beekeeper import Beekeeper

if TYPE_CHECKING:
    from pathlib import Path


def check_log_json_rpc(log_json_rpc_path: Path) -> None:
    """Check if log-json-rpc directory has been created, and contains required files."""
    json_1_path = log_json_rpc_path / "1.json"
    json_1_pat_path = log_json_rpc_path / "1.json.pat"

    assert json_1_path.exists()
    assert json_1_pat_path.exists()


async def test_log_json_rpc(tmp_path: Path) -> None:
    """Test will check command line flag --log-json-rpc."""
    # ARRANGE
    tempdir = tmp_path / "test_log_json_rpc"
    tempdir.mkdir()

    # ACT
    async with await Beekeeper().launch(log_json_rpc=tempdir):
        # ASSERT
        check_log_json_rpc(tempdir)
