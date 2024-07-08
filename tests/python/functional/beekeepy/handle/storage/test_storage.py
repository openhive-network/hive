from __future__ import annotations

import json
import shutil
from pathlib import Path

import pytest
from beekeepy._handle import Beekeeper
from beekeepy.exceptions import BeekeeperAlreadyRunningError
from beekeepy.settings import Settings
from helpy import HttpUrl

import test_tools as tt
from hive_local_tools.beekeeper import checkers


@pytest.fixture()
def storage_path() -> Path:
    return tt.context.get_current_directory()


def prepare_directory(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path)
    path.mkdir()


def test_multiply_beekeepeer_same_storage(storage_path: Path) -> None:
    """Test test_multiply_beekeepeer_same_storage will check, if it is possible to run multiple instances of beekeepers pointing to the same storage."""
    # ARRANGE
    same_storage = storage_path / "same_storage"
    prepare_directory(same_storage)
    settings = Settings(working_directory=same_storage)

    # ACT & ASSERT 1
    with Beekeeper(settings=settings, logger=tt.logger) as bk1:
        assert bk1.is_running is True, "First instance of beekeeper should launch without any problems."

        # ACT & ASSERT 2
        bk2 = Beekeeper(settings=settings, logger=tt.logger)
        with pytest.raises(BeekeeperAlreadyRunningError) as err:
            bk2.run()
        assert err.value.pid == bk1.pid, "PID is pointing to invalid process"
        assert err.value.address == bk1.http_endpoint, "Address is pointing to invalid endpoint"

        assert checkers.check_for_pattern_in_file(
            bk2.settings.working_directory / "stderr.log",
            "Failed to lock access to wallet directory; is another `beekeeper` running?",
        ), "There should be an info about another instance of beekeeper locking wallet directory."


def test_multiply_beekeepeer_different_storage(storage_path: Path) -> None:
    """Test test_multiply_beekeepeer_different_storage will check, if it is possible to run multiple instances of beekeepers pointing to the different storage."""
    # ARRANGE
    bk1_path = storage_path / "bk1"
    prepare_directory(bk1_path)

    bk2_path = storage_path / "bk2"
    prepare_directory(bk2_path)

    # ACT
    bks: list[Beekeeper] = []
    with Beekeeper(settings=Settings(working_directory=bk1_path), logger=tt.logger) as bk1, Beekeeper(
        settings=Settings(working_directory=bk2_path), logger=tt.logger
    ) as bk2:
        # ASSERT
        assert bk1.is_running, "First instance of beekeeper should be working."
        assert bk2.is_running, "Second instance of beekeeper should be working."
        bks.extend((bk1, bk2))

    for bk in bks:
        assert (
            checkers.check_for_pattern_in_file(
                bk.settings.working_directory / "stderr.log",
                "Failed to lock access to wallet directory; is another `beekeeper` running?",
            )
            is False
        ), "There should be an no info about another instance of beekeeper locking wallet directory."


def get_remote_address_from_connection_file(working_dir: Path) -> HttpUrl:
    connection: dict[str, str | int] = {}
    with (working_dir / "beekeeper.connection").open() as file:
        connection = json.load(file)
    return HttpUrl(
        f"{connection['address']}:{connection['port']}",
        protocol=str(connection["type"]).lower(),  # type: ignore
    )


def test_beekeepers_files_generation(beekeeper: Beekeeper) -> None:
    """Test test_beekeepers_files_generation will check if beekeeper files are generated and have same content."""
    # ARRANGE & ACT
    wallet_dir = beekeeper.settings.working_directory
    beekeeper_connection_file = wallet_dir / "beekeeper.connection"
    beekeeper_pid_file = wallet_dir / "beekeeper.pid"
    beekeeper_wallet_lock_file = wallet_dir / "beekeeper.wallet.lock"

    # ASSERT
    assert beekeeper_connection_file.exists() is True, "File 'beekeeper.connection' should exists"
    assert beekeeper_pid_file.exists() is True, "File 'beekeeper.pid' should exists"
    # File beekeeper.wallet.lock holds no value inside, so we need only to check is its exists.
    assert beekeeper_wallet_lock_file.exists() is True, "File 'beekeeper.wallet.lock' should exists"

    connection_url = get_remote_address_from_connection_file(wallet_dir)
    assert connection_url is not None, "There should be connection details."

    if beekeeper.http_endpoint.address == "127.0.0.1":
        assert connection_url.address in [
            "0.0.0.0",
            "127.0.0.1",
        ], "Address should point to localhost or all interfaces."
    else:
        assert connection_url.address == beekeeper.http_endpoint.address, "Host should be the same."
    assert connection_url.port == beekeeper.http_endpoint.port, "Port should be the same."
    assert connection_url.protocol == beekeeper.http_endpoint.protocol, "Protocol should be the same."

    with Path.open(beekeeper_pid_file) as pid:
        content = json.load(pid)

        assert content["pid"] == str(beekeeper.pid), "Pid should be the same"
