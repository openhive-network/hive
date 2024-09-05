from __future__ import annotations

from functools import wraps
from typing import TYPE_CHECKING, Iterator

import pytest
from beekeepy import Settings
from loguru import logger

import test_tools as tt

if TYPE_CHECKING:
    from loguru import Logger

    from hive_local_tools.beekeeper.models import SettingsFactory, SettingsLoggerFactory


@pytest.fixture()
def settings() -> SettingsFactory:
    @wraps(settings)
    def _factory(settings_update: Settings | None = None) -> Settings:
        context_dir = tt.context.get_current_directory()
        amount_of_beekeepers_in_context = len([x for x in context_dir.glob("Beekeeper*") if x.is_dir()])
        working_dir = context_dir / f"Beekeeper{amount_of_beekeepers_in_context}"
        result = settings_update or Settings()
        result.working_directory = working_dir
        return result

    return _factory


@pytest.fixture()
def settings_with_logger(request: pytest.FixtureRequest, settings: SettingsFactory) -> Iterator[SettingsLoggerFactory]:
    handlers_to_remove = []

    @wraps(settings_with_logger)
    def _factory(settings_update: Settings | None = None) -> tuple[Settings, Logger]:
        sets = settings(settings_update)
        test_name = request.node.name
        log = logger.bind(test_name=test_name)
        handlers_to_remove.append(
            log.add(
                sets.working_directory / "beekeeper.log",
                filter=lambda params: params["extra"].get("test_name") == test_name,
            )
        )
        return sets, log

    yield _factory

    for hid in handlers_to_remove:
        logger.remove(hid)
