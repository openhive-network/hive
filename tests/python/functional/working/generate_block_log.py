from pathlib import Path

import test_tools as tt
from python.functional.comment_cashout_tests.block_log.generate_block_log import generate_block


def prepare_block_log():
    """
    - 2 witnessts ( initminer, witness-0 ),
    - 50k empty blocks,
    """
    node = tt.InitNode()
    node.config.block_log_split = -1
    node.run()

    wallet = tt.Wallet(attach_to=node)

    # Prepare witness
    key = tt.PublicKey("initminer")
    wallet.api.create_account_with_keys("initminer", "witness-0", "{}", key, key, key, key)
    wallet.api.transfer_to_vesting("initminer", "witness-0", tt.Asset.Test(1000))

    wallet.api.update_witness(
        "witness-0",
        "https://" + "witness-0",
        tt.Account("initminer").public_key,
        {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "hbd_interest_rate": 0},
    )

    generate_block(node, 50_000)

    node.close()
    wallet.close()
    node.block_log.copy_to(Path(__file__).parent.absolute())
    tt.logger.info("FINISH")


if __name__ == "__main__":
    prepare_block_log()
