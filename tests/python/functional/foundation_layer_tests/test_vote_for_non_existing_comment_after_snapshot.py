import os

import test_tools as tt

from hive_local_tools.functional.python import generate_block


def test_vote_for_non_existing_comment_after_snapshot(block_log_empty_430_split: tt.BlockLog) -> None:
    """
    Na "num":22,"lib":1
       "num":23,"lib":2
       "num":29,"lib":8
       "num":30,"lib":30,
    symbols: str = "■ ▶ ◆"

    - świeże,
    - wypłacone (ale blok w którym nastąpiła nie jest irreversible),
    - zmigrowane ( czyki blok w którym nastąpiła wypłata jest reversible)

    obi można wyłączyć poprzez nie aktywowanie hf26

        0   |                            20
        │---|-----------◆----------------│
    ────●───────────────◆────────────────■────────▶[t]
    start               ?            stop

    """
    acs = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
        init_witnesses=[f"witness-{witness_num}" for witness_num in range(14)]
    )

    node = tt.InitNode()
    node.config.plugin.append("queen")
    node.config.block_log_split = 9999
    node.config.plugin.append("witness_api")

    node.run(alternate_chain_specs=acs)

    for i in range(10):
        generate_block(node, 1)

    wallet = tt.Wallet(attach_to=node)
    ca = wallet.api.create_account("initminer", "alice", "{}", broadcast=False)
    node.api.network_broadcast.broadcast_transaction(trx=ca)
    generate_block(node, 1)

    ###
    node.api.witness.disable_fast_confirm()
    ###

    for i in range(100):
        generate_block(node, 1)

    trx = wallet.api.post_comment(
            "alice",
            "test-permlink",
            "",
            "test-parent-permlink",
            "test-title",
            "test-body",
            "{}",
            broadcast=False
        )

    aa = node.api.wallet_bridge.broadcast_transaction_synchronous(trx)
    # 3 typy komentarzy: świeże, wypłacone (ale blok w którym nastąpiła nie jest irreversible) i zmigrowane
    print()
