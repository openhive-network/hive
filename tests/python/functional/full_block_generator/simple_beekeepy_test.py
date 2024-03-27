from __future__ import annotations

import time

import wax

import test_tools as tt

from beekeepy import beekeeper_factory

from test_tools.__private.account import Account
from tests.functional.full_block_generator.generate_transaction_template import SimpleTransaction, \
    generate_transaction_template
from beekeepy.settings import Settings
from schemas.operations.transfer_operation import TransferOperation
from concurrent.futures import ProcessPoolExecutor
from beekeepy import beekeeper_factory, PackedBeekeeper


def test_simply():
    node = tt.InitNode()
    node.run()

    beekeeper = beekeeper_factory(settings=Settings(working_directory=tt.context.get_current_directory() / "beekeeper"))
    with beekeeper.create_session() as session:
        with session.create_wallet(name="wallet", password="password") as wallet:
            wallet.import_key(private_key=tt.Account("alice").private_key)
            wallets = []
            for _ in range(63):
                session = beekeeper.create_session()
                wallets.append(session.open_wallet(name="wallet", password="password"))

            public_key = wallet.generate_key()
            assert public_key in wallet.public_keys
            wallet.import_key(private_key=Account("initminer").private_key)

            gdpo = node.api.database.get_dynamic_global_properties()
            node_config = node.api.database.get_config()

            trx: SimpleTransaction = generate_transaction_template(gdpo)

            trx.add_operation(TransferOperation(
                from_="initminer",
                to="initminer",
                amount=tt.Asset.Test(0.1),
                memo="{}",
            ))
            sig_digest_from_wax = wax.calculate_sig_digest(
                trx.json(by_alias=True).encode("ascii"), node_config.HIVE_CHAIN_ID.encode("ascii")
            ).result.decode("ascii")
            signature = wallet.sign_digest(sig_digest=sig_digest_from_wax, key=tt.PublicKey("initminer")[3:])

            trx.signatures.append(signature)
            print()

    beekeeper.delete()
    pass


def test_S():
    beekeeper = beekeeper_factory(settings=Settings(working_directory=tt.context.get_current_directory() / "beekeeper"))
    with beekeeper.create_session() as session:
        with session.create_wallet(name="wallet", password="password") as wallet:
            public_key = wallet.generate_key()
            assert public_key in wallet.public_keys
            wallet.import_key(private_key=tt.Account("initminer").private_key)

    beekeeper.delete()
    pass

def test_bee():
    beekeeper = beekeeper_factory(settings=Settings(working_directory=tt.context.get_current_directory() / "beekeeper"))

    sessions = []
    wallets = []
    for _ in range(10):
        if _ == 0:
            sessions.append(beekeeper.create_session())
            a = sessions[-1].get_info()
            wallets.append(sessions[-1].create_wallet(name="wallet", password="password"))
        else:
            sessions.append(beekeeper.create_session())
            b = sessions[-1].get_info()
            wallets.append(sessions[-1].open_wallet(name="wallet", password="password"))
        print()


def import_new_key(packed: PackedBeekeeper, wallet: str, password: str) -> None:
    with packed.unpack() as bk:
        with bk.create_session() as ss:
            with ss.open_wallet(name=wallet, password=password) as wlt:
                wlt.generate_key()

def test_packing() -> None:
    name, password = "wallet", "password"
    with beekeeper_factory(settings=Settings(working_directory=tt.context.get_current_directory() / "beekeeper")) as bk:
        for _ in range(10):
            a = bk.create_session()
            print()



        with bk.create_session() as ss:
            with ss.create_wallet(name=name, password=password) as wlt:
                assert len(wlt.public_keys) == 0
                with ProcessPoolExecutor() as exec_a:
                    future = exec_a.submit(import_new_key, bk.pack(), name, password)
                    assert future.exception() is None
                    print()
