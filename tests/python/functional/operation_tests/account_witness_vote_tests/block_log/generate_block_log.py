from __future__ import annotations

from pathlib import Path

import test_tools as tt

WITNESSES = ["witness-x", "witness-y"]


def prepare_blocklog_with_witnesses():
    node = tt.InitNode()

    for name in WITNESSES:
        node.config.witness.append(name)
        node.config.private_key.append(tt.Account(name).private_key)

    node.run()
    node.set_vest_price(tt.Asset.Vest(1800))

    wallet = tt.Wallet(attach_to=node)

    for name in WITNESSES:
        wallet.api.import_key(tt.PrivateKey(name))

    # Add witnesses to blockchain
    with wallet.in_single_transaction():
        for name in WITNESSES:
            key = tt.PublicKey(name)
            wallet.api.create_account_with_keys("initminer", name, "{}", key, key, key, key)
    with wallet.in_single_transaction():
        for name in WITNESSES:
            wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(1000))
    with wallet.in_single_transaction():
        for name in WITNESSES:
            wallet.api.update_witness(
                name,
                "https://" + name,
                tt.Account(name).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "hbd_interest_rate": 0},
            )

    tt.logger.info("Wait 21 blocks to schedule newly created witnesses into future slate")
    node.wait_number_of_blocks(21)

    future_witnesses = node.api.database.get_active_witnesses(include_future=True)["future_witnesses"]
    tt.logger.info(f"Future witnesses after voting: {future_witnesses}")

    tt.logger.info("Wait 21 blocks for future slate to become active slate")
    node.wait_number_of_blocks(21)

    active_witnesses = node.api.database.get_active_witnesses()["witnesses"]
    tt.logger.info(f"Witness state after voting: {active_witnesses}")

    # Reason of this wait is to enable moving forward of irreversible block
    tt.logger.info("Wait 21 blocks (when every witness sign at least one block)")
    node.wait_number_of_blocks(21)

    timestamp = node.api.block.get_block(block_num=node.get_last_block_number())["block"]["timestamp"]
    tt.logger.info(f"Final block_log head block number: {node.get_last_block_number()}")
    tt.logger.info(f"Final block_log head block timestamp: {timestamp}")

    with open("timestamp", "w", encoding="utf-8") as file:
        file.write(f"{timestamp}")

    node.close()
    node.block_log.copy_to(Path(__file__).parent.absolute())


if __name__ == "__main__":
    prepare_blocklog_with_witnesses()
