from __future__ import annotations

from datetime import datetime
from pathlib import Path
from typing import Final

from dynaconf import Dynaconf  # type: ignore

ROOT_DIRECTORY: Final[Path] = Path(__file__).parent.parent
TESTS_DIRECTORY: Final[Path] = ROOT_DIRECTORY.parent / "tests"
LAUNCH_TIME: Final[datetime] = datetime.now()
_DATA_DIRECTORY: Final[Path] = ROOT_DIRECTORY / ".clive"

# order matters - later paths override earlier values for the same key of earlier paths
SETTINGS_FILES: Final[list[str]] = ["settings.toml", str(_DATA_DIRECTORY / "settings.toml")]

settings = Dynaconf(
    envvar_prefix="CLIVE",
    root_path=ROOT_DIRECTORY,
    settings_files=SETTINGS_FILES,
    environments=True,
    # preconfigured settings
    data_path=_DATA_DIRECTORY,
)

settings.LOG_DIRECTORY = Path(".")
settings.LOG_PATH = _DATA_DIRECTORY if not settings.LOG_DIRECTORY else Path(settings.LOG_DIRECTORY)
settings.LOG_PATH /= "logs/"
