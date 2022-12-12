import math
from concurrent.futures import ThreadPoolExecutor, as_completed
from copy import deepcopy
from pathlib import Path
from typing import Any, Callable, Final, Iterable, List, Optional, Sequence

import test_tools as tt

AMOUNT_OF_ALL_ACCOUNTS: Final[int] = 2000
ACCOUNTS_PER_CHUNK: Final[int] = 500
MAX_WORKERS: Final[int] = 6


def execute_function_in_threads(
    function: Callable,
    *,
    amount: int,
    args: Optional[Iterable] = None,
    args_sequences: Optional[Iterable[Sequence]] = None,
    chunk_size: Optional[int] = None,
    max_workers: Optional[int] = None,
) -> None:
    """
    Execute the given function in threads.

    Example:
        letters = ['a', 'b', 'c', 'd']

        def function(x: int, y: float, z: List[str]) -> None:
            print(x, y, *z)

        execute_function_in_threads(
            function, amount=len(letters), args=(1, 2.0), args_sequences=(letters,), chunk_size=2
        )

        Output:
            1 2.0 a b
            1 2.0 c d

    :param function: Function to be called.
    :param args: Arguments for the function. If given, the function will be called with the same arguments for
        each thread.
    :param args_sequences: Sequenced arguments for the function (like tuple or list). If given, the function will be
        called with sequenced arguments, automatically dividing their length by chunk size, evenly distributing the work
        among workers. See the example above.
    :param amount: Amount of times the function will be called or length of the sequences given in the args_sequential
        parameter (so there will be `n = amount/chunk_size` calls), but only when chunk_size was specified.
    :param chunk_size: Size of the chunks. Should be specified if the args_sequential parameter is given.
    :param max_workers: Maximum amount of workers used in ThreadPoolExecutor. Defaults ThreadPoolExecutor's default.
    """
    args = args or []
    args_sequences = args_sequences or []
    chunk_size = chunk_size or 1
    max_workers = math.inf if chunk_size and not max_workers else max_workers

    with ThreadPoolExecutor(max_workers=min(math.ceil(amount / chunk_size), max_workers)) as executor:
        futures = []
        for lower in range(0, amount, chunk_size):
            upper = min(amount + 1, lower + chunk_size)

            sequences = [sequence[lower:upper] for sequence in args_sequences]
            futures.append(executor.submit(function, *args, *sequences))

            detail = f" with elements of index: <{lower}:{upper - 1}>" if chunk_size > 1 else f": {lower + 1}/{amount}"
            tt.logger.info(f"Pack submitted{detail}")

        for index, future in enumerate(as_completed(futures)):
            tt.logger.info(f"Pack finished: {index + 1}/{len(futures)}")
            if exception := future.exception():
                for f in futures:
                    f.cancel()
                raise exception


def prepare_block_log() -> None:
    init_node = tt.InitNode()
    init_node.config.shared_file_size = "16G"
    init_node.run()

    wallet = tt.Wallet(attach_to=init_node)

    wallet.create_account("bank")

    account_names = [account.name for account in wallet.create_accounts(AMOUNT_OF_ALL_ACCOUNTS)]
    tt.logger.info(f"{len(account_names)} accounts successfully created!")

    execute_function_in_threads(
        __generate_and_broadcast,
        args=(wallet,),
        args_sequences=(account_names,),
        amount=AMOUNT_OF_ALL_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )

    tt.logger.info("Waiting 21 blocks for the block with the transaction to become irreversible")
    init_node.wait_number_of_blocks(21)
    head_block_num = init_node.get_last_block_number()
    timestamp = init_node.api.block.get_block(block_num=head_block_num)["block"]["timestamp"]
    tt.logger.info(f"head block timestamp: {timestamp}")

    with open("timestamp", "w", encoding="utf-8") as file:
        file.write(f"{timestamp}")

    init_node.close()
    init_node.block_log.copy_to(Path(__file__).parent)


def __generate_operations_for_receiver(receiver: str, amount: tt.Asset.Test = tt.Asset.Test(100)) -> list:
    return [
        [
            "transfer",
            {"from": "initminer", "to": receiver, "amount": str(amount), "memo": f"supply_transfer-{receiver}"},
        ],
        [
            "recurrent_transfer",
            {
                "from": receiver,
                "to": "bank",
                "amount": str(tt.Asset.Test(0.001)),
                "memo": f"rec_transfer-{receiver}",
                "recurrence": 24,
                "executions": 2,
                "extensions": [],
            },
        ],
    ]


def __generate_and_broadcast(wallet: tt.Wallet, account_names: List[str]) -> None:
    transaction_template = {
        "ref_block_num": 0,
        "ref_block_prefix": 0,
        "expiration": "2022-11-23T11:55:36",
        "operations": [],
        "extensions": [],
        "signatures": [],
        "transaction_id": "0000000000000000000000000000000000000000",
        "block_num": 0,
        "transaction_num": 0,
    }

    transaction = deepcopy(transaction_template)

    for name in account_names:
        transaction["operations"].extend(__generate_operations_for_receiver(name))

    wallet.api.sign_transaction(transaction)

    tt.logger.info(f"Finished: {account_names[-1]}")


if __name__ == "__main__":
    prepare_block_log()
