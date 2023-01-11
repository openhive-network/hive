from concurrent.futures import ThreadPoolExecutor
from copy import deepcopy
import math
from pathlib import Path
from typing import Final, List

import test_tools as tt

from hive_local_tools.constants import TRANSACTION_TEMPLATE

AMOUNT_OF_ALL_ACCOUNTS: Final[int] = 20_000
ACCOUNTS_PER_CHUNK: Final[int] = 750
MAX_WORKERS: Final[int] = 6


def prepare_node_with_proposal_votes():
    node = tt.InitNode()
    node.run()
    wallet = tt.Wallet(attach_to=node)

    account_names = [account.name for account in wallet.create_accounts(AMOUNT_OF_ALL_ACCOUNTS)]

    __create_proposal(wallet)

    with ThreadPoolExecutor(
        max_workers=min(math.ceil(AMOUNT_OF_ALL_ACCOUNTS / ACCOUNTS_PER_CHUNK), MAX_WORKERS)
    ) as executor:
        for lower in range(0, AMOUNT_OF_ALL_ACCOUNTS, ACCOUNTS_PER_CHUNK):
            upper = min(AMOUNT_OF_ALL_ACCOUNTS, lower + ACCOUNTS_PER_CHUNK)
            executor.submit(__generate_and_broadcast, wallet, account_names[lower:upper])
            tt.logger.info(f"Pack generated: {lower}:{upper}")

    tt.logger.info("Waiting for the blocks with the transactions to become irreversible")
    node.wait_for_irreversible_block()

    head_block_num = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num=head_block_num)["block"]["timestamp"]
    tt.logger.info(f"head block timestamp: {timestamp}")

    with open("timestamp", "w", encoding="utf-8") as file:
        file.write(f"{timestamp}")

    node.close()
    node.block_log.copy_to(Path(__file__).parent)


def __create_proposal(wallet: tt.Wallet) -> None:
    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))
    wallet.api.post_comment("alice", "permlink", "", "parent-permlink", "title", "body", "{}")
    wallet.api.create_proposal(
        "alice",
        "alice",
        tt.Time.now(),
        tt.Time.from_now(days=10),
        tt.Asset.Tbd(1 * 100),
        "subject",
        "permlink",
    )


def __generate_operations_for_receiver(receiver: str, amount: tt.Asset.Test = tt.Asset.Test(0.1)) -> list:
    return [
        ["transfer_to_vesting", {"from": "initminer", "to": receiver, "amount": str(amount)}],
        ["update_proposal_votes", {"voter": receiver, "proposal_ids": [0], "approve": True, "extensions": []}],
    ]


def __generate_and_broadcast(wallet: tt.Wallet, account_names: List[str]) -> None:
    transaction = deepcopy(TRANSACTION_TEMPLATE)

    for name in account_names:
        transaction["operations"].extend(__generate_operations_for_receiver(name))

    wallet.api.sign_transaction(transaction)
    tt.logger.info(f"Finished: {account_names[-1]}")


if __name__ == "__main__":
    prepare_node_with_proposal_votes()
