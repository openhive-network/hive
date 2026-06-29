from __future__ import annotations

from copy import deepcopy
from pathlib import Path
from typing import Final

import test_tools as tt
from hive_local_tools.constants import TRANSACTION_TEMPLATE
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from schemas.operations.transfer_to_vesting_operation import TransferToVestingOperation
from schemas.operations.update_proposal_votes_operation import UpdateProposalVotesOperation
from test_tools.__private.wallet.constants import SimpleTransactionLegacy

AMOUNT_OF_ALL_ACCOUNTS: Final[int] = 20_000
ACCOUNTS_PER_CHUNK: Final[int] = 300
MAX_WORKERS: Final[int] = 6


def prepare_node_with_proposal_votes():
    node = tt.InitNode()
    node.config.block_log_split = -1
    node.run()
    wallet = tt.Wallet(attach_to=node)
    # long expiration so the threaded bulk broadcasts below do not expire while many keys are being signed
    wallet.api.set_transaction_expiration(3600 - 1)

    account_names = [account.name for account in wallet.create_accounts(AMOUNT_OF_ALL_ACCOUNTS)]

    __create_proposal(wallet)

    execute_function_in_threads(
        __generate_and_broadcast,
        args=(wallet,),
        args_sequences=(account_names,),
        amount=AMOUNT_OF_ALL_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )

    tt.logger.info("Waiting for the blocks with the transactions to become irreversible")
    node.wait_for_irreversible_block()

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


def __generate_operations_for_receiver(receiver: str) -> list:
    return [
        TransferToVestingOperation(from_="initminer", to=receiver, amount=tt.Asset.Test(0.1)),
        UpdateProposalVotesOperation(voter=receiver, proposal_ids=[0], approve=True, extensions=[]),
    ]


def __generate_and_broadcast(wallet: tt.Wallet, account_names: list[str]) -> None:
    transaction = SimpleTransactionLegacy(**deepcopy(TRANSACTION_TEMPLATE))

    for name in account_names:
        for operation in __generate_operations_for_receiver(name):
            transaction.add_operation(operation)

    wallet.api.sign_transaction(transaction)
    tt.logger.info(f"Finished: {account_names[-1]}")


if __name__ == "__main__":
    prepare_node_with_proposal_votes()
