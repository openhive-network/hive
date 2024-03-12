from __future__ import annotations

import test_tools as tt
from hive_local_tools.functional.python.operation import create_transaction_with_any_operation
from schemas.operations import CustomJsonOperation


def test_custom_json_operation():
    node = tt.InitNode()
    node.run()
    wallet = tt.Wallet(attach_to=node)
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    cs = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=["alice"],
        id_=1,
        json_='{"a": 1, "b": 2, "c": 3}',
    )

    cs2 = CustomJsonOperation(
        required_auths=[],
        required_posting_auths=["alice"],
        id_=1,
        json_='{"a": 11, "b": 22, "c": 33}',
    )

    trx = create_transaction_with_any_operation(wallet, cs, cs2)
    print(trx)
