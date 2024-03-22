from concurrent.futures import ProcessPoolExecutor
from beekeepy import beekeeper_factory, PackedBeekeeper


def import_new_key(packed: PackedBeekeeper, wallet: str, password: str) -> None:
    with packed.unpack() as bk:
        with bk.create_session() as ss:
            with ss.open_wallet(name=wallet, password=password) as wlt:
                wlt.generate_key()

def test_packing() -> None:
    name, password = "wallet", "password"
    with beekeeper_factory() as bk:
        with bk.create_session() as ss:
            with ss.create_wallet(name=name, password=password) as wlt:
                assert len(wlt.public_keys) == 0
                with ProcessPoolExecutor() as exec:
                    future = exec.submit(import_new_key, bk.pack(), name, password)
                    assert future.exception() is None
