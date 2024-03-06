from __future__ import annotations

import shutil

import pytest
# import test_tools as tt
from beekeepy._handle import Beekeeper as BeekeeperHandle
# from test_tools.__private.scope.scope_fixtures import *  # noqa: F403


@pytest.fixture()
def beekeeper() -> BeekeeperHandle:
    pass
    # beekeeper_wdir = tt.context.get_current_directory() / "beekeeper"
    # if beekeeper_wdir.exists():
    #     shutil.rmtree(beekeeper_wdir)
    # beekeeper_wdir.mkdir()
    # tt.logger.add(
    #     beekeeper_wdir / "beekeeper_handle.log",
    #     filter=lambda args: args["extra"].get("beekeeper_path") == beekeeper_wdir,
    # )
    # return BeekeeperHandle(working_directory=beekeeper_wdir, logger=tt.logger.bind(beekeeper_path=beekeeper_wdir))
