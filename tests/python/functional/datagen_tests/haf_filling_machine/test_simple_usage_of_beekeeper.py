from __future__ import annotations

import time
from concurrent.futures import ProcessPoolExecutor
import helpy
from shutil import rmtree
from multiprocessing import Process, Lock, Value

rmtree("/home/dev/hive/tests/python/functional/datagen_tests/haf_filling_machine/.clive/beekeeper", ignore_errors=True)

import test_tools as tt
import wax
from schemas.operations import TransferOperation

from .beekeeper import Beekeeper
from .generate_transaction_template import (
    SimpleTransaction,
    generate_transaction_template,
)

password = "password"
wallet = "aaaa"


def create_wallet(x: int, addr: str, imported_key: str, lck: Lock):
    try:
        beek = Beekeeper()
        beek.launch()

        node = tt.RemoteNode(http_endpoint=helpy.HttpUrl(addr))
        initminer = tt.Account("initminer")
        wn = f"{wallet}_{x}"
        beek.api.create(wallet_name=wn, password=password)
        beek.api.import_key(wallet_name=wn, wif_key=initminer.private_key)

        gdpo = node.api.database.get_dynamic_global_properties()
        node_config = node.api.database.get_config()
        trx: SimpleTransaction = generate_transaction_template(gdpo)
        trx.add_operation(TransferOperation(from_=initminer.name, to="null", amount=tt.Asset.Hive(1), memo=f"test{x}"))
        sig_digest = wax.calculate_sig_digest(
            trx.json(by_alias=True).encode("ascii"), node_config.HIVE_CHAIN_ID.encode("ascii")
        ).result.decode("ascii")
        trx.signatures.append(beek.api.sign_digest(sig_digest=sig_digest, public_key=imported_key).signature)
        result = None
        while result is None:
            try:
                result = node.api.network_broadcast.broadcast_transaction(trx=trx)
            except:
                pass
    finally:
        lck.value = 1


def test_simple_usage_of_beekeeper(node: tt.InitNode):
    with Beekeeper() as bk:
        initminer = tt.Account("initminer")

        bk.api.create(wallet_name=wallet, password=password)
        imported_key = bk.api.import_key(wallet_name=wallet, wif_key=initminer.private_key).public_key

        print(bk.api.get_public_keys().keys)

        gdpo = node.api.database.get_dynamic_global_properties()
        node_config = node.api.database.get_config()
        trx: SimpleTransaction = generate_transaction_template(gdpo)
        trx.add_operation(TransferOperation(from_=initminer.name, to="null", amount=tt.Asset.Hive(1), memo="test"))
        sig_digest = wax.calculate_sig_digest(
            trx.json(by_alias=True).encode("ascii"), node_config.HIVE_CHAIN_ID.encode("ascii")
        ).result.decode("ascii")
        trx.signatures.append(bk.api.sign_digest(sig_digest=sig_digest, public_key=imported_key).signature)
        node.api.network_broadcast.broadcast_transaction(trx=trx)

        workers = 4
        xxx = []
        for i in range(workers):
            v = Value('d', 0)
            xxx.append((Process(target=create_wallet, args=(i, node.http_endpoint.as_string(), imported_key, v)), v))
            xxx[-1][0].start()

        for x in xxx:
            while  x[1].value != 1:
                time.sleep(0.5)
            x[0].kill()


        print('dupa')
