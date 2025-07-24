from __future__ import annotations

import math
import os
from pathlib import Path
from typing import TYPE_CHECKING, Final

import test_tools as tt
from hive_local_tools.constants import MAX_OPEN_RECURRENT_TRANSFERS, MAX_RECURRENT_TRANSFERS_PER_BLOCK
from hive_local_tools.functional import __generate_and_broadcast_transaction
from hive_local_tools.functional.python import generate_block
from hive_local_tools.functional.python.block_log_generation import parse_block_log_generator_args
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from hive_local_tools.functional.python.queen_utils import (
    __create_account,
    __recurrent_transfer,
    __transfer,
    __vesting_delegate,
)
from schemas.fields.assets import AssetHive
from schemas.policies import DisableSwapTypes, set_policies

if TYPE_CHECKING:
    from datetime import datetime

AMOUNT_OF_ALL_ACCOUNTS: Final[int] = 30_000
ACCOUNTS_PER_CHUNK: Final[int] = 256
MAX_WORKERS: Final[int] = os.cpu_count() * 4
WITNESSES: Final[str] = [f"witness-{num}" for num in range(20)]
INIT_SUPPLY: Final[int] = 400_000_000_000
INITIAL_VESTING: Final[int] = 50_000_000_000
HBD_INIT_SUPPLY: Final[int] = 30_000_000_000

set_policies(DisableSwapTypes(disabled=True))


def prepare_block_log(output_block_log_directory: Path) -> None:
    """
    This script generate block_log with specific conditions. The entire block_log can be divided into four parts:
      1) creating accounts - the number is specified in a variable - AMOUNT_OF_ALL_ACCOUNTS
      2) fund accounts - each account is credited with HIVE and VESTS
      3) order recurring transfers - each of the created accounts orders the maximum number of recurring transfers (255)
      4) waiting to be processed recurrent transfers.
    """
    output_block_log_directory.mkdir(parents=True, exist_ok=True)

    node = tt.InitNode()
    node.config.shared_file_size = "16G"
    node.config.plugin.append("queen")
    for witness in WITNESSES:
        node.config.witness.append(witness)

    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    tt.logger.disable("helpy")
    tt.logger.disable("test_tools")

    acs = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[
            tt.HardforkSchedule(hardfork=18, block_num=1),
            tt.HardforkSchedule(hardfork=current_hardfork_number, block_num=100),
        ],
        init_supply=INIT_SUPPLY,
        hbd_init_supply=HBD_INIT_SUPPLY,
        initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=INITIAL_VESTING),
        init_witnesses=WITNESSES,
    )
    node.run(alternate_chain_specs=acs)
    generate_block(node, 1)  # need to activate hardforks

    wallet = tt.OldWallet(attach_to=node)
    wallet.api.set_transaction_expiration(3600 - 1)
    witnesses = wallet.api.list_witnesses("", 100)

    with wallet.in_single_transaction(broadcast=False) as tx:
        # this is required to minimize inflation impact on vest price
        wallet.api.transfer_to_vesting_nonblocking(from_="initminer", to="initminer", amount=tt.Asset.Test(200_000_000))
        # change maximum block size limit to 2 mb
        for witness in witnesses:
            wallet.api.update_witness(
                witness,
                "http://url.html",
                tt.Account("initminer").public_key,
                {"account_creation_fee": tt.Asset.Test(0), "maximum_block_size": 2097152, "hbd_interest_rate": 1000},
            )
    node.api.network_broadcast.broadcast_transaction(trx=tx.get_response())
    generate_block(node, 43)

    # create accounts
    key: Final[tt.PrivateKey] = tt.PrivateKey("secret")
    wallet.api.import_key(key)
    tt.logger.info("Started creating accounts...")
    account_names = [f"account-{num}" for num in range(AMOUNT_OF_ALL_ACCOUNTS)]
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, node, __create_account),
        args_sequences=(account_names,),
        amount=AMOUNT_OF_ALL_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info(f"{len(account_names)} accounts successfully created!")

    # transfer liquid
    tt.logger.info("Started transfer HIVE to accounts...")

    liquid_per_account = (
        node.api.database.find_accounts(accounts=["initminer"]).accounts[0].balance / AMOUNT_OF_ALL_ACCOUNTS
    ).amount
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, node, __transfer),
        args_sequences=(account_names,),
        kwargs={"transfer_asset": tt.Asset.Test(int(liquid_per_account) // 1000)},
        amount=AMOUNT_OF_ALL_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info("Finish transfer HIVE to accounts!")

    # vesting delegate
    vs_per_account = (
        node.api.database.find_accounts(accounts=["initminer"]).accounts[0].get_vesting() / AMOUNT_OF_ALL_ACCOUNTS
    )
    tt.logger.info("Started vesting delegate to accounts...")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, node, __vesting_delegate),
        args_sequences=(account_names,),
        kwargs={"vests_amount": (vs_per_account - tt.Asset.Vest(10)).as_legacy()},
        amount=AMOUNT_OF_ALL_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info("Finish vesting delegate!")
    tt.logger.info(f"maximum_block_size: {node.api.database.get_witness_schedule().median_props.maximum_block_size}")

    time_before_operations = __get_head_block_time(node)
    wallet.close()

    assert node.api.database.get_hardfork_properties().last_hardfork == 18
    tt.logger.info(f"Start waiting for current hardfork! @Block: {node.get_last_block_number()}")
    while node.api.database.get_hardfork_properties().last_hardfork != current_hardfork_number:
        generate_block(node, 1)
    assert node.api.database.get_hardfork_properties().last_hardfork == current_hardfork_number
    tt.logger.info(f"Finish waiting for current hardfork! @Block: {node.get_last_block_number()}")

    # create recurrent transfers
    # after activating the current hardfork version, a wallet that supports hf_26 serialization is required
    wallet_hf26 = tt.Wallet(attach_to=node)
    wallet_hf26.api.import_key(key)
    tt.logger.info("Started creating recurrent transfers...")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet_hf26, node, __recurrent_transfer),
        args_sequences=(account_names,),
        kwargs={"amount": AssetHive(amount=1), "executions": 2, "recurrence": 24},
        amount=AMOUNT_OF_ALL_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )

    # wait for recurrent transfers to be processed
    all_accounts_names = wallet_hf26.list_accounts()
    blocks_to_wait = math.ceil(
        len(all_accounts_names) * MAX_OPEN_RECURRENT_TRANSFERS / MAX_RECURRENT_TRANSFERS_PER_BLOCK
    )
    with node.temporarily_change_timeout(seconds=360):
        generate_block(node, blocks_to_wait)

    tt.logger.info("Finish creating recurrent transfers!")

    tt.logger.info(f"Resource_pool: {node.api.rc.get_resource_pool()}")
    assert __get_head_block_time(node) - time_before_operations < tt.Time.hours(24)
    tt.logger.info(f"Last block number after apply all rt: {node.get_last_block_number()}")

    head_block_num = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num=head_block_num).block.timestamp
    tt.logger.info(f"head block number: {head_block_num}")
    tt.logger.info(f"head block timestamp: {timestamp}")

    acs.export_to_file(output_dir=output_block_log_directory)
    wallet_hf26.close()
    node.close()
    node.block_log.copy_to(output_block_log_directory / "recurrent_many_to_many")


def __get_head_block_time(node: tt.AnyNode) -> datetime:
    last_block_number = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num=last_block_number).block.timestamp
    return tt.Time.parse(timestamp)


if __name__ == "__main__":
    args = parse_block_log_generator_args()
    prepare_block_log(args.output_block_log_directory)
