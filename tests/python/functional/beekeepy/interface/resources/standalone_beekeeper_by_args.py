from __future__ import annotations

from pathlib import Path
from sys import argv

from beekeepy import Beekeeper, Settings, close_already_running_beekeeper

settings = Settings(working_directory=Path(argv[1]))

if argv[2] == "start":
    bk = Beekeeper.factory(settings=settings)
    bk.detach()
elif argv[2] == "stop":
    close_already_running_beekeeper(working_directory=settings.working_directory)
