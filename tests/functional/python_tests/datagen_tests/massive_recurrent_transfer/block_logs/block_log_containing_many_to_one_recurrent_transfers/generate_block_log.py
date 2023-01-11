from copy import deepcopy
from pathlib import Path
from typing import Final, List

import test_tools as tt

from hive_local_tools.constants import TRANSACTION_TEMPLATE
from hive_local_tools.functional.python.datagen.massive_recurrent_transfer import execute_function_in_threads

NUMBER_OF_SENDER_ACCOUNTS: Final[int] = 50_000
ACCOUNTS_PER_CHUNK: Final[int] = 500
MAX_WORKERS: Final[int] = 6
RECEIVER_ACCOUNT_NAME: Final[str] = "receiver"
SINGLE_TRANSFER_AMOUNT: Final[tt.Asset.Test] = tt.Asset.Test(1)

def prepare_block_log_with_many_to_one_recurrent_transfers() -> None:
    """
    This script generate block_log with specific conditions:
      1) create a receiver account
      2) create accounts - the number is specified in a constant - NUMBER_OF_ACCOUNTS
      3) fund accounts - each account is credited with HIVE
      4) order recurring transfers - each of the created accounts order one recurring transfer to receiver account
    """
    init_node = tt.InitNode()
    init_node.config.shared_file_size = "16G"
    init_node.run()

    wallet = tt.Wallet(attach_to=init_node)

    wallet.create_account(RECEIVER_ACCOUNT_NAME)

    account_names = [account.name for account in wallet.create_accounts(NUMBER_OF_SENDER_ACCOUNTS)]
    tt.logger.info(f"{len(account_names)} accounts successfully created!")

    execute_function_in_threads(
        __generate_and_broadcast,
        args=(wallet,),
        args_sequences=(account_names,),
        amount=NUMBER_OF_SENDER_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )

    # waiting for the block with the last transaction to become irreversible
    init_node.wait_for_irreversible_block()

    head_block_num = init_node.get_last_block_number()
    timestamp = init_node.api.block.get_block(block_num=head_block_num)["block"]["timestamp"]
    tt.logger.info(f"head block timestamp: {timestamp}")

    with open("timestamp", "w", encoding="utf-8") as file:
        file.write(f"{timestamp}")

    init_node.close()
    init_node.block_log.copy_to(Path(__file__).parent)


def __generate_operations_for_receiver(account: str) -> list:
    return [
        [
            "transfer",
            {"from": "initminer", "to": account, "amount": str(tt.Asset.Test(4)), "memo": f"supply_transfer-{account}"},
        ],
        [
            "recurrent_transfer",
            {
                "from": account,
                "to": RECEIVER_ACCOUNT_NAME,
                "amount": str(SINGLE_TRANSFER_AMOUNT),
                "memo": f"rec_transfer-{account}",
                "recurrence": 24,
                "executions": 2,
                "extensions": [],
            },
        ],
    ]


def __generate_and_broadcast(wallet: tt.Wallet, account_names: List[str]) -> None:
    transaction = deepcopy(TRANSACTION_TEMPLATE)

    for name in account_names:
        transaction["operations"].extend(__generate_operations_for_receiver(name))

    wallet.api.sign_transaction(transaction)

    tt.logger.info(f"Finished: {account_names[-1]}")


if __name__ == "__main__":
    prepare_block_log_with_many_to_one_recurrent_transfers()
