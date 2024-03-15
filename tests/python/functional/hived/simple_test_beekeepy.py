from __future__ import annotations

from beekeepy import beekeeper_factory
from test_tools.__private.account import Account


def test_simply():
    beekeeper = beekeeper_factory()
    with beekeeper.create_session() as session:
        with session.create_wallet(name="wallet", password="password") as wallet:
            public_key = wallet.generate_key()
            assert public_key in wallet.public_keys
            wallet.import_key(private_key=Account("initminer").private_key)

    beekeeper.delete()
    pass
