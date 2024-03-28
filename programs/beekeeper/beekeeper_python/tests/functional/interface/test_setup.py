from __future__ import annotations

import pytest
import test_tools as tt

from beekeepy import beekeeper_factory
from beekeepy.settings import Settings


@pytest.fixture()
def settings() -> Settings:
    return Settings(working_directory=tt.context.get_current_directory() / "beekeeper")

def test_smoke_interface(settings: Settings) -> None:
    with beekeeper_factory(settings=settings) as bk, bk.create_session() as session, session.create_wallet(name="test", password="password") as wallet:
        pub_key = wallet.generate_key()
        assert pub_key in wallet.public_keys


def test_closing_with_delete(settings: Settings) -> None:
    bk = beekeeper_factory(settings=settings)
    bk.delete()

def test_closing_with_with(settings: Settings) -> None:
    with beekeeper_factory(settings=settings):
        pass

def test_session_tokens(settings: Settings) -> None:
    with beekeeper_factory(settings=settings) as bk:  # noqa: SIM117
        with bk.create_session() as s1, bk.create_session() as s2:
            assert s1.token != s2.token
