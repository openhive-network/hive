from __future__ import annotations

from beekeepy import beekeeper_factory


def test_smoke_interface() -> None:
    with beekeeper_factory() as bk, bk.create_session() as session, session.create_wallet(name="test", password="password") as wallet:
        pub_key = wallet.generate_key()
        assert pub_key in wallet.public_keys


def test_closing_with_delete() -> None:
    bk = beekeeper_factory()
    bk.delete()

def test_closing_with_with() -> None:
    with beekeeper_factory():
        pass

def test_session_tokens() -> None:
    with beekeeper_factory() as bk:
        with bk.create_session() as s1, bk.create_session() as s2:
            assert s1.token != s2.token
