from __future__ import annotations

from typing import TYPE_CHECKING

from beekeepy import beekeeper_factory

if TYPE_CHECKING:
    from python.functional.beekeepy.settings_factory import SettingsFactory


def test_smoke_interface(settings: SettingsFactory) -> None:
    with beekeeper_factory(settings=settings()) as bk, bk.create_session() as session, session.create_wallet(
        name="test", password="password"
    ) as wallet:
        pub_key = wallet.generate_key()
        assert pub_key in wallet.public_keys, "Public key not in wallet public keys"


def test_closing_with_delete(settings: SettingsFactory) -> None:
    bk = beekeeper_factory(settings=settings())
    bk.delete()


def test_closing_with_with(settings: SettingsFactory) -> None:
    with beekeeper_factory(settings=settings()):
        pass


def test_session_tokens(settings: SettingsFactory) -> None:
    with beekeeper_factory(settings=settings()) as bk:  # noqa: SIM117
        with bk.create_session() as s1, bk.create_session() as s2:
            assert s1.token != s2.token, "Tokens are not unique"
