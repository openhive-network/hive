from __future__ import annotations

from copy import deepcopy

import test_tools as tt
from hive_local_tools.constants import TRANSACTION_TEMPLATE


def test_multi_sig():
    node = tt.InitNode()
    node.run()

    wallet = tt.Wallet(attach_to=node)

    transaction = deepcopy(TRANSACTION_TEMPLATE)

    owners = [[tt.Account("alice", secret=f"owner-{key}").public_key, 1] for key in range(3)]
    wallet.api.import_keys([tt.Account("alice", secret=f"owner-{key}").private_key for key in range(3)])

    active = [[tt.Account("alice", secret=f"active-{key}").public_key, 1] for key in range(6)]
    wallet.api.import_keys([tt.Account("alice", secret=f"active-{key}").private_key for key in range(6)])

    posting = [[tt.Account("alice", secret=f"posting-{key}").public_key, 1] for key in range(10)]
    wallet.api.import_keys([tt.Account("alice", secret=f"posting-{key}").private_key for key in range(10)])

    op = [
        [
            "account_create",
            {
                "fee": tt.Asset.Test(0).as_legacy(),
                "creator": "initminer",
                "new_account_name": "alice",
                "owner": {"weight_threshold": 3, "account_auths": [], "key_auths": owners},
                "active": {"weight_threshold": 6, "account_auths": [], "key_auths": active},
                "posting": {"weight_threshold": 10, "account_auths": [], "key_auths": posting},
                "memo_key": tt.Account("alice").public_key,
                "json_metadata": "",
            },
        ],
    ]
    transaction["operations"] = op
    wallet.api.sign_transaction(transaction, broadcast=True)

    wallet.api.transfer_to_vesting("initminer", "alice", tt.Asset.Test(1000))
    wallet.api.transfer("initminer", "alice", tt.Asset.Test(1000), "{}")
    wallet.api.transfer("initminer", "alice", tt.Asset.Tbd(1000), "{}")

    # custom json
    wallet.api.use_authority("posting", "alice")
    custom_json = wallet.api.follow("alice", "alice", ["blog"])
    assert len(custom_json["signatures"]) == len(posting)

    # comment ( posting )
    wallet.api.use_authority("posting", "alice")
    comment = wallet.api.post_comment("alice", "test-permlink", "", "someone0", "test-title", "this is a body", "{}")
    assert len(comment["signatures"]) == len(posting)

    # vote ( posting )
    wallet.api.use_authority("posting", "alice")
    vote = wallet.api.vote("alice", "alice", "test-permlink", 100)
    assert len(vote["signatures"]) == len(posting)

    # transfer ( active )
    wallet.api.use_automatic_authority()
    wallet.api.use_authority("active", "alice")
    transfer = wallet.api.transfer("alice", "initminer", tt.Asset.Test(1), "{}", broadcast=False)
    assert len(transfer["signatures"]) == len(active)

    # recurrent transfer ( active )
    recurrent_transfer = wallet.api.recurrent_transfer("alice", "initminer", tt.Asset.Test(1), "memo", 24, 5)
    assert len(recurrent_transfer["signatures"]) == len(active)

    print()
