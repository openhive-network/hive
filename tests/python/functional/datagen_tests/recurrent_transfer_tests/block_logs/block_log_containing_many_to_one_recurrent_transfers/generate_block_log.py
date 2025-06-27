from __future__ import annotations

import os
from pathlib import Path
from typing import Final

import test_tools as tt
from hive_local_tools.functional import __generate_and_broadcast_transaction
from hive_local_tools.functional.python import generate_block
from hive_local_tools.functional.python.block_log_generation import parse_block_log_generator_args
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from schemas.fields.assets import AssetHive


from hive_local_tools.functional.python.queen_utils import (
    __create_account,
    __recurrent_transfer,
    __transfer,
    __transfer_to_vesting,
)

NUMBER_OF_SENDER_ACCOUNTS: Final[int] = 50_000
ACCOUNTS_PER_CHUNK: Final[int] = 500
MAX_WORKERS: Final[int] = os.cpu_count()
RECEIVER_ACCOUNT_NAME: Final[str] = "receiver"
SINGLE_TRANSFER_AMOUNT: Final[AssetHive] = AssetHive(amount=AssetNaiAmount(1)),


def prepare_block_log_with_many_to_one_recurrent_transfers(output_block_log_directory: Path) -> None:
    """
    This script generate block_log with specific conditions:
      1) create a receiver account
      2) create accounts - the number is specified in a constant - NUMBER_OF_ACCOUNTS
      3) fund accounts - each account is credited with HIVE
      4) order recurring transfers - each of the created accounts order one recurring transfer to receiver account
    """
    output_block_log_directory.mkdir(parents=True, exist_ok=True)

    init_node = tt.InitNode()
    init_node.config.shared_file_size = "16G"
    init_node.config.plugin.append("queen")

    acs = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
    )
    init_node.run(alternate_chain_specs=acs)
    generate_block(init_node, 1)  # need to activate hardforks

    wallet = tt.Wallet(attach_to=init_node)
    wallet.api.import_key(tt.PrivateKey("secret"))

    with wallet.in_single_transaction(blocking=False):
        wallet.api.create_account("initminer", RECEIVER_ACCOUNT_NAME, "{}")
    generate_block(init_node, 1)

    # create accounts
    tt.logger.info("Started creating accounts...")
    account_names = [f"account-{num}" for num in range(NUMBER_OF_SENDER_ACCOUNTS)]
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, init_node, __create_account),
        args_sequences=(account_names,),
        amount=NUMBER_OF_SENDER_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(init_node, 1)
    tt.logger.info(f"{len(account_names)} accounts successfully created!")

    # transfer liquid
    tt.logger.info("Started transfer HIVE to accounts...")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, init_node, __transfer),
        args_sequences=(account_names,),
        kwargs={"transfer_asset": AssetHive(amount=4)},
        amount=NUMBER_OF_SENDER_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(init_node, 1)
    tt.logger.info("Finish transfer HIVE to accounts!")

    # vesting
    tt.logger.info("Started vesting HIVE to accounts...")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, init_node, __transfer_to_vesting),
        args_sequences=(account_names,),
        kwargs={"transfer_asset":AssetHive(amount=10)},
        amount=NUMBER_OF_SENDER_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(init_node, 1)
    tt.logger.info("Finish vesting!")

    # create recurrent transfers
    tt.logger.info("Started transfer HIVE to accounts...")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, init_node, __recurrent_transfer),
        args_sequences=(account_names,),
        kwargs={"receiver": RECEIVER_ACCOUNT_NAME, "amount": SINGLE_TRANSFER_AMOUNT},
        amount=NUMBER_OF_SENDER_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    # Wait a few blocks to ensure all recurrent transfers are written to the block log before continuing.
    generate_block(init_node, 100)
    tt.logger.info("Finish recurrent transfers to accounts!")

    head_block_num = init_node.get_last_block_number()
    timestamp = init_node.api.block.get_block(block_num=head_block_num).block.timestamp
    tt.logger.info(f"head block timestamp: {timestamp}")

    wallet.close()
    init_node.close()
    init_node.block_log.copy_to(output_block_log_directory / "recurrent_many_to_one")


if __name__ == "__main__":
    args = parse_block_log_generator_args()
    prepare_block_log_with_many_to_one_recurrent_transfers(args.output_block_log_directory)
