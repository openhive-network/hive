from __future__ import annotations

import json
import shutil
import tempfile
from pathlib import Path
from typing import TYPE_CHECKING

from beekeepy._executable.beekeeper_config import BeekeeperConfig
from beekeepy._executable.beekeeper_executable_discovery import get_beekeeper_binary_path
from beekeepy._executable.executable import Executable
from beekeepy.settings import Settings
from helpy import KeyPair

if TYPE_CHECKING:
    from loguru import Logger


class BeekeeperExecutable(Executable[BeekeeperConfig]):
    def __init__(self, settings: Settings, logger: Logger) -> None:
        super().__init__(settings.binary_path or get_beekeeper_binary_path(), settings.working_directory, logger)

    def _construct_config(self) -> BeekeeperConfig:
        return BeekeeperConfig(wallet_dir=self.working_directory)

    def export_keys_wallet(
        self, wallet_name: str, wallet_password: str, extract_to: Path | None = None
    ) -> list[KeyPair]:
        with tempfile.TemporaryDirectory() as tempdir:
            tempdir_path = Path(tempdir) if not isinstance(tempdir, Path) else tempdir
            wallet_file_name = f"{wallet_name}.wallet"
            shutil.copyfile(self.working_directory / wallet_file_name, tempdir_path / wallet_file_name)
            bk = BeekeeperExecutable(
                settings=Settings(binary_path=get_beekeeper_binary_path(), working_directory=self.working_directory),
                logger=self._logger,
            )
            bk.run(
                blocking=True,
                arguments=[
                    "-d",
                    tempdir_path.as_posix(),
                    "--notifications-endpoint",
                    "0.0.0.0:0",
                    "--export-keys-wallet",
                    json.dumps([wallet_name, wallet_password]),
                ],
            )

        keys_path = bk.working_directory / f"{wallet_name}.keys"
        if extract_to is not None:
            shutil.move(keys_path, extract_to)
            keys_path = extract_to

        with keys_path.open("r") as file:
            return [KeyPair(**obj) for obj in json.load(file)]
