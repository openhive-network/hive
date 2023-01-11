from __future__ import annotations

from copy import deepcopy
from datetime import datetime
import math
from pathlib import Path
from typing import Final, List

import test_tools as tt

from hive_local_tools.constants import TRANSACTION_TEMPLATE
from hive_local_tools.functional import VestPrice
from hive_local_tools.functional.python.datagen.massive_recurrent_transfer import execute_function_in_threads

AMOUNT_OF_ALL_ACCOUNTS: Final[int] = 30_000
ACCOUNTS_PER_CHUNK: Final[int] = 256
MAX_WORKERS: Final[int] = 100


def prepare_block_log():
    """
    This script generate block_log with specific conditions. The entire block_log can be divided into four parts:
      1) creating accounts - the number is specified in a variable - AMOUNT_OF_ALL_ACCOUNTS
      2) fund accounts - each account is credited with HIVE and VESTS
      3) order recurring transfers - each of the created accounts orders the maximum number of recurring transfers (255)
      4) waiting to be processed recurrent transfers.
    """
    node = tt.InitNode()
    node.config.shared_file_size = "16G"
    node.run(time_offset="+0 x15")

    wallet = tt.Wallet(attach_to=node)

    # this is required to minimanize inflation impact on vest price
    wallet.api.transfer_to_vesting(
        from_='initminer',
        to='initminer',
        amount=tt.Asset.Test(10_000)
    )

    # change maximum block size limit to 2 mb
    wallet.api.update_witness('initminer', 'http://url.html',
                              tt.Account('initminer').public_key,
                              {'account_creation_fee': tt.Asset.Test(0), 'maximum_block_size': 2097152,
                               'hbd_interest_rate': 1000})

    new_price = VestPrice(quote=tt.Asset.Vest(1800), base=tt.Asset.Test(1))
    tt.logger.info(f"new vests price {new_price}.")
    tt.logger.info(f"new vests price {new_price.as_nai()}.")
    node.api.debug_node.debug_set_vest_price(vest_price=new_price.as_nai())

    account_names = [account.name for account in wallet.create_accounts(AMOUNT_OF_ALL_ACCOUNTS)]

    execute_function_in_threads(
        __fund_account_and_broadcast,
        args=(wallet,),
        args_sequences=(account_names,),
        amount=AMOUNT_OF_ALL_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    tt.logger.info("All accounts founded")

    tt.logger.info(f"created accounts: {node.api.condenser.get_account_count()}")
    tt.logger.info(f"maximum_block_size: {node.api.database.get_witness_schedule()['median_props']['maximum_block_size']}")

    time_before_operations = __get_head_block_time(node)

    execute_function_in_threads(
        __order_and_broadcast_recurrent_transfers,
        args=(wallet,),
        args_sequences=(account_names,),
        amount=AMOUNT_OF_ALL_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )

    tt.logger.info(f'resource_pool: {node.api.rc.get_resource_pool()}')
    assert __get_head_block_time(node) - time_before_operations < tt.Time.hours(24)

    tt.logger.info("Waiting for the block with the last transaction to become irreversible...")
    node.wait_for_irreversible_block()
    tt.logger.info(f"Last block number after apply all rt: {node.get_last_block_number()}")

    waiting_to_block_with_number = math.ceil(AMOUNT_OF_ALL_ACCOUNTS * 255 / 1000)
    tt.logger.info(f"Waiting till block {waiting_to_block_with_number} for pending recurrent transfers to be processed...")
    node.wait_for_block_with_number(waiting_to_block_with_number)

    head_block_num = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num=head_block_num)["block"]["timestamp"]
    tt.logger.info(f"head block number: {head_block_num}")
    tt.logger.info(f"head block timestamp: {timestamp}")

    with open("timestamp", "w", encoding="utf-8") as file:
        file.write(f"{timestamp}")

    node.close()
    node.block_log.copy_to(Path(__file__).parent)


def __generate_recurrent_transfers_for_sender(sender: str, all_accounts: List[str], amount: tt.Asset.Test = tt.Asset.Test(0.001)) -> list:
    operations=[]
    for receiver in all_accounts:
        if sender != receiver:
            operations.append([
                "recurrent_transfer",
                {
                    "from": sender,
                    "to": receiver,
                    "amount": str(amount),
                    "memo": f"rec_transfer-{sender}",
                    "recurrence": 24,
                    "executions": 2,
                    "extensions": [],
                },
            ])
    return operations


def __order_and_broadcast_recurrent_transfers(wallet: tt.Wallet, account_names: List[str]) -> None:
    for name in account_names:
        transaction = deepcopy(TRANSACTION_TEMPLATE)
        transaction["operations"].extend(__generate_recurrent_transfers_for_sender(name, account_names))
        wallet.api.sign_transaction(transaction)
        tt.logger.info(f"Finished: {name}")


def __fund_account_and_broadcast(wallet: tt.Wallet, account_names: List[str]) -> None:
    transaction = deepcopy(TRANSACTION_TEMPLATE)
    transaction["operations"].extend(__fund_account(account_names))
    wallet.api.sign_transaction(transaction)
    tt.logger.info(f"Finished: {account_names[-1]}")


def __fund_account(account_names):
    operations = []
    for account in account_names:
        transfer = ['transfer', {'from': 'initminer', 'to': account, 'amount': tt.Asset.Test(300), 'memo': '{}'}]
        transfer_to_vesting = ['transfer_to_vesting', {'from': "initminer", 'to': account, 'amount': tt.Asset.Test(4600)}]
        operations.append(transfer)
        operations.append(transfer_to_vesting)
    return operations


def __get_head_block_time(node: tt.AnyNode) -> datetime:
    last_block_number = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num=last_block_number)["block"]["timestamp"]
    return tt.Time.parse(timestamp)


if __name__ == "__main__":
    prepare_block_log()
