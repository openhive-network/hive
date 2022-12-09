from concurrent.futures import as_completed, ThreadPoolExecutor
from copy import deepcopy
from pathlib import Path
from typing import Final, List
import math

import test_tools as tt

AMOUNT_OF_ALL_ACCOUNTS: Final[int] = 50_000
ACCOUNTS_PER_CHUNK: Final[int] = 500
MAX_WORKERS: Final[int] = 6

def prepare_block_log() -> None:
    init_node = tt.InitNode()
    init_node.config.shared_file_size = '16G'
    init_node.run()

    wallet = tt.Wallet(attach_to=init_node)

    account_names = [account.name for account in wallet.create_accounts(AMOUNT_OF_ALL_ACCOUNTS)]

    wallet.create_account("bank")
    wallet.api.recurrent_transfer()

    tt.logger.info(f"Account succesfully created! {len(account_names)}")

    futures = []
    with ThreadPoolExecutor(
        max_workers=min(math.ceil(AMOUNT_OF_ALL_ACCOUNTS / ACCOUNTS_PER_CHUNK), MAX_WORKERS)
    ) as executor:
        for idx, lower in enumerate(range(0, AMOUNT_OF_ALL_ACCOUNTS, ACCOUNTS_PER_CHUNK)):
            upper = min(AMOUNT_OF_ALL_ACCOUNTS, lower + ACCOUNTS_PER_CHUNK)
            futures.append(executor.submit(__generate_and_broadcast, wallet, account_names[lower:upper], idx))
            tt.logger.info(f"Pack generated: {lower}:{upper}")

        for idx, future in enumerate(as_completed(futures)):
            try:
                tt.logger.info(f'waiting for: {idx}')
                future.result()
            except Exception as e:
                tt.logger.info(f"Got exception from {idx}")
            else:
                tt.logger.info(f'result merged: {idx}')

            # if exception := future.exception():
            #     raise ValueError()
    init_node.wait_number_of_blocks(21)
    head_block_num = init_node.get_last_block_number()
    timestamp = init_node.api.block.get_block(block_num=head_block_num)["block"]["timestamp"]
    tt.logger.info(f"head block timestamp: {timestamp}")

    with open("timestamp", "w", encoding="utf-8") as file:
        file.write(f"{timestamp}")

    init_node.close()
    init_node.block_log.copy_to(Path(__file__).parent.absolute())


def __generate_operations_for_receiver(receiver: str, amount: tt.Asset.Test = tt.Asset.Test(100)) -> list:
    return [
        ['transfer', {'from': 'initminer', 'to': receiver, 'amount': str(amount), 'memo': f'supply_transfer-{receiver}'}],
        ['recurrent_transfer', {'from': receiver, 'to': 'bank', 'amount': str(tt.Asset.Test(0.001)), 'memo': f'rec_transfer-{receiver}', 'recurrence': 24, 'executions': 2, 'extensions': []}]
    ]

def __generate_and_broadcast(wallet: tt.Wallet, account_names: List[str], idx) -> None:
    # raise Exception('__generate_and_broadcast exception')
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

    try:
        wallet.api.sign_transaction(transaction)
    except Exception as exception:
        tt.logger.info(f"__generate_and_broadcast: {idx}")
        raise exception

    tt.logger.info(f"Finished: {account_names[-1]}")


if __name__ == '__main__':
    try:
        prepare_block_log()
    except Exception as exception:
        tt.logger.info(f'__name__ exception: {exception}')
        raise exception