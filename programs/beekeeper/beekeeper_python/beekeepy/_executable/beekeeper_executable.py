from __future__ import annotations

import json
import shutil
from typing import TYPE_CHECKING

from beekeepy._executable.beekeeper_config import BeekeeperConfig
from beekeepy._executable.beekeeper_executable_discovery import get_beekeeper_binary_path
from beekeepy._executable.executable import Executable
from beekeepy.settings import Settings
from helpy import KeyPair

if TYPE_CHECKING:
    from pathlib import Path

    from loguru import Logger


class BeekeeperExecutable(Executable[BeekeeperConfig]):
    def __init__(self, settings: Settings, logger: Logger) -> None:
        super().__init__(settings.binary_path or get_beekeeper_binary_path(), settings.working_directory, logger)

    def _construct_config(self) -> BeekeeperConfig:
        return BeekeeperConfig(wallet_dir=self.woring_dir)

    def export_keys_wallet(
        self, wallet_name: str, wallet_password: str, extract_to: Path | None = None
    ) -> list[KeyPair]:
        tempdir = self.woring_dir / "export-keys-workdir"
        if tempdir.exists():
            shutil.rmtree(tempdir)
        tempdir.mkdir()

        shutil.copy(self.woring_dir / f"{wallet_name}.wallet", tempdir)
        bk = BeekeeperExecutable(
            settings=Settings(binary_path=get_beekeeper_binary_path(), working_directory=self.woring_dir),
            logger=self._logger,
        )
        bk.run(
            blocking=True,
            arguments=[
                "-d",
                tempdir.as_posix(),
                "--notifications-endpoint",
                "0.0.0.0:0",
                "--export-keys-wallet",
                json.dumps([wallet_name, wallet_password]),
            ],
        )

        keys_path = bk.woring_dir / f"{wallet_name}.keys"
        if extract_to:
            extract_to.write_text(keys_path.read_text())

        with keys_path.open("r") as file:
            return [KeyPair(**obj) for obj in json.load(file)]
