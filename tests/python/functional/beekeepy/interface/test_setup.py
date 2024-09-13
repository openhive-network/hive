from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy import Beekeeper

if TYPE_CHECKING:
    from hive_local_tools.beekeeper.models import SettingsFactory


def test_smoke_interface(settings: SettingsFactory) -> None:
    # ARRANGE
    with Beekeeper.factory(settings=settings()) as bk, bk.create_session() as session, session.create_wallet(
        name="test", password="password"
    ) as wallet:
        # ACT
        pub_key = wallet.generate_key()

        # ASSERT
        assert pub_key in wallet.public_keys, "Public key not in wallet public keys"


def test_closing_with_delete(settings: SettingsFactory) -> None:
    # ARRANGE
    sets = settings()
    bk = Beekeeper.factory(settings=sets)

    # ACT & ASSERT (no throw)
    bk.teardown()
    assert not (sets.working_directory / "beekeeper.pid").exists()


def test_closing_with_with(settings: SettingsFactory) -> None:
    # ARRANGE, ACT & ASSERT (no throw)
    sets = settings()
    with Beekeeper.factory(settings=sets):
        assert (sets.working_directory / "beekeeper.pid").exists()


def test_session_tokens(settings: SettingsFactory) -> None:
    # ARRANGE
    with Beekeeper.factory(settings=settings()) as bk:  # noqa: SIM117
        # ACT
        with bk.create_session() as s1, bk.create_session() as s2:
            # ASSERT
            assert s1.token != s2.token, "Tokens are not unique"
