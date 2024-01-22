from __future__ import annotations

from concurrent.futures import ProcessPoolExecutor

import helpy

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


def create_wallet(x: int, addr: str, imported_key: str):
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
    node.api.network_broadcast.broadcast_transaction(trx=trx)


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
        futures = []
        with ProcessPoolExecutor(max_workers=workers) as exec:
            for i in range(workers):
                futures.append(exec.submit(create_wallet, i, node.http_endpoint.as_string(), imported_key))

            for x in futures:
                assert x.exception() is None

            exec.shutdown(cancel_futures=True, wait=False)
