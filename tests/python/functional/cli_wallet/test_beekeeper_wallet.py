from __future__ import annotations

import test_tools as tt
from test_tools.__private.account import Account


def test_first():
    node = tt.InitNode()
    node.run()

    tt.Wallet(attach_to=node)
    # wallet.create_account("alice")
    # wallet.api.create_account("initminer", "alice", "{}")

    b_wallet = tt.BeekeeperWallet(attach_to=node)

    b_wallet.api.import_key(wif_key=Account("initminer").private_key)

    b_wallet.api.create_account("initminer", "alice", "{}")
    with b_wallet.in_single_transaction() as trx:
        b_wallet.api.post_comment(
            "initminer", "test-permlink", "", "test-parent-permlink", "test-title", "test-body", "{}"
        )
        b_wallet.api.post_comment(
            "alice", "test-permlink1", "", "test-parent-permlink1", "test-title1", "test-body1", "{}"
        )

    # with wallet.in_single_transaction() as trx:
    #     wallet.api.post_comment("initminer", "test-permlink", "", "test-parent-permlink", "test-title", "test-body", "{}")
    #     wallet.api.post_comment("alice", "test-permlink1", "", "test-parent-permlink1", "test-title1", "test-body1", "{}")

    tt.logger.info(f"transakcja: {trx.get_response()}")
