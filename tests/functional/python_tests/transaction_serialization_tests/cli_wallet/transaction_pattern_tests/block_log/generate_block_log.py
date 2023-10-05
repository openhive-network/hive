from pathlib import Path

import test_tools as tt


def prepare_blocklog():
    node = tt.InitNode()
    node.run()

    wallet = tt.Wallet(attach_to=node, additional_arguments=["--transaction-serialization=legacy"])

    wallet.create_account("alice", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000000), hbds=tt.Asset.Tbd(1000))
    wallet.create_account("bob", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000000), hbds=tt.Asset.Tbd(1000))

    ####################################################################################################################
    # Cancel_transfer_from_savings preparation
    wallet.api.transfer_to_savings("initminer", "alice", tt.Asset.Test(10), "memo")
    wallet.api.transfer_from_savings("alice", 1, "bob", tt.Asset.Test(1), "memo")

    # Cancel_order preparation
    wallet.api.create_order("alice", 1, tt.Asset.Test(1), tt.Asset.Tbd(1), False, 1000)

    # Create_proposal preparation
    wallet.api.post_comment("alice", "permlink", "", "parent-permlink", "title", "body", "{}")

    # Encrow release preparation
    wallet.api.escrow_transfer(
        "initminer",
        "alice",
        "bob",
        1,
        tt.Asset.Tbd(10),
        tt.Asset.Test(10),
        tt.Asset.Tbd(10),
        "2031-01-01T00:00:00",
        "2031-06-01T00:00:00",
        "{}",
    )
    wallet.api.escrow_approve("initminer", "alice", "bob", "bob", 1, True)
    wallet.api.escrow_approve("initminer", "alice", "bob", "alice", 1, True)
    wallet.api.escrow_dispute("initminer", "alice", "bob", "initminer", 1)

    # Encrow approve preparation
    wallet.api.escrow_transfer(
        "initminer",
        "alice",
        "bob",
        2,
        tt.Asset.Tbd(10),
        tt.Asset.Test(10),
        tt.Asset.Tbd(10),
        "2031-01-01T00:00:00",
        "2031-06-01T00:00:00",
        "{}",
    )

    # Encrow dispute preparation
    wallet.api.escrow_transfer(
        "initminer",
        "alice",
        "bob",
        3,
        tt.Asset.Tbd(10),
        tt.Asset.Test(10),
        tt.Asset.Tbd(10),
        "2031-01-01T00:00:00",
        "2031-06-01T00:00:00",
        "{}",
    )
    wallet.api.escrow_approve("initminer", "alice", "bob", "bob", 3, True)
    wallet.api.escrow_approve("initminer", "alice", "bob", "alice", 3, True)

    # Recover account preparation
    wallet.api.change_recovery_account("initminer", "alice")

    # Remove proposal preparation
    wallet.api.create_proposal(
        "alice", "alice", "2031-01-01T00:00:00", "2031-06-01T00:00:00", tt.Asset.Tbd(1000), "subject-1", "permlink"
    )

    ####################################################################################################################

    node.wait_for_irreversible_block()
    node.close()
    node.block_log.copy_to(Path(__file__).parent.absolute())


if __name__ == "__main__":
    prepare_blocklog()
