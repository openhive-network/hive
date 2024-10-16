from __future__ import annotations

import argparse
import random
from copy import deepcopy
from pathlib import Path
from typing import Final, Literal

from loguru import logger

import test_tools as tt
from hive_local_tools.constants import (
    HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD,
    TRANSACTION_TEMPLATE,
)
from hive_local_tools.functional import wait_for_current_hardfork
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from hive_local_tools.functional.python.operation import get_vesting_price

CHAIN_ID: Final[int] = 24

NUMBER_OF_ACCOUNTS: Final[int] = 2_000_000
NUMBER_OF_COMMENTS: Final[int] = 100_000  # There are active posts from account-0 to account-99999
ACCOUNT_NAMES = [f"account-{account}" for account in range(NUMBER_OF_ACCOUNTS)]
WITNESSES = [f"witness-{w}" for w in range(20)]

INIT_SUPPLY: Final[int] = 400_000_000_000
INITIAL_VESTING: Final[int] = 50_000_000_000
HBD_INIT_SUPPLY: Final[int] = 30_000_000_000

DELEGATION_PER_ACCOUNT: Final[tt.Asset.Vest] = tt.Asset.Vest(1750 * INITIAL_VESTING // 1000 // NUMBER_OF_ACCOUNTS)
HIVE_PER_ACCOUNT: Final[tt.Asset.Test] = tt.Asset.Test((INIT_SUPPLY - INITIAL_VESTING) // 1000 // NUMBER_OF_ACCOUNTS)
TBD_PER_ACCOUNT: Final[tt.Asset.Tbd] = tt.Asset.Tbd(HBD_INIT_SUPPLY // 1000 // NUMBER_OF_ACCOUNTS)
INVEST_PER_ACCOUNT: Final[tt.Asset.Test] = HIVE_PER_ACCOUNT / 3
INITIAL_ACCOUNT_CREATION_FEE: Final[tt.Asset.Test] = tt.Asset.Test(0.001)
ACCOUNT_CREATION_FEE_AFTER_HF_20: Final[tt.Asset.Test] = tt.Asset.Test(3)

ACCOUNTS_PER_CHUNK: Final[int] = 1024
MAX_WORKERS: Final[int] = 6


def prepare_block_log(
    output_block_log_directory: Path,
    signature_type: Literal["open_sign", "multi_sign", "single_sign"],
    activate_current_hf: int,
) -> None:
    """
    This script generate block_log with specific conditions:
      1) start blockchain with hardfork: 18 ( before introduction RC ),
      2) set and stabilize price hive to vest ( ~1800 vest per 1 hive ),
      3) activate 21 witnesses ( witnesses-0 - witnesses-19 + initminer ),
      4) change block size to maximum value 2mb,
      5) create accounts - the number is specified in a constant - NUMBER_OF_ACCOUNTS,
      6) fund accounts - each account is credited with HIVE and HBD ( from initminer ),
      7) each account do transfer_to_vesting,
      8) initminer delegate vesting shares to each account ( vests from price stabilization ),
      9) each account create post,
      10) wait for activate current hardfork,
      11) restart node unlock `delayed votes` after vesting delegation.
    """
    block_log_directory = output_block_log_directory / f"block_log_{signature_type}"

    logger.disable("helpy")
    logger.disable("test_tools")

    node = tt.InitNode()
    node.config.shared_file_size = "24G"
    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    five_days_in_blocks = 5 * 24 * 3600 // 3

    block_log_config = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[
            tt.HardforkSchedule(hardfork=18, block_num=0),
            tt.HardforkSchedule(hardfork=current_hardfork_number, block_num=activate_current_hf + five_days_in_blocks),
        ],
        init_supply=INIT_SUPPLY,
        hbd_init_supply=HBD_INIT_SUPPLY,
        initial_vesting={"hive_amount": INITIAL_VESTING, "vests_per_hive": 1800},
        init_witnesses=WITNESSES,
        min_root_comment_interval=3,
    )
    block_log_config.export_to_file(block_log_directory)

    node.run(alternate_chain_specs=block_log_config, arguments=[f"--chain-id={CHAIN_ID}"])
    tt.logger.info(f"Price Hive/Vest: {get_vesting_price(node)}")

    wallet = tt.Wallet(attach_to=node, additional_arguments=[f"--chain-id={CHAIN_ID}"])
    wallet.api.set_transaction_expiration(3600 - 1)

    witnesses = wallet.api.list_witnesses("", 100)
    with wallet.in_single_transaction():
        for witness in witnesses:
            # change block size to 2mb
            wallet.api.update_witness(
                witness,
                "https://" + witness,
                tt.Account(witness).public_key,
                {
                    # fee is high because the number of `create proposal operations` performed increases the RC costs
                    "account_creation_fee": INITIAL_ACCOUNT_CREATION_FEE,
                    "maximum_block_size": 2097152,
                    "hbd_interest_rate": 0,
                },
            )

    tt.logger.info("Wait 43 blocks...")
    node.wait_number_of_blocks(43)  # wait for the block size to change to 2mb

    authority = generate_authority(wallet, signature_type)

    # create_accounts, fund hbd and hive
    tt.logger.info(f"Start creating accounts! @Block: {node.get_last_block_number()}")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __create_account_and_fund_hive_and_hbd,
            wallet,
            authority,
        ),
        args_sequences=(ACCOUNT_NAMES,),
        amount=NUMBER_OF_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK - 512,
        max_workers=MAX_WORKERS,
    )
    tt.logger.info(f"Finish creating accounts! @Block: {node.get_last_block_number()}")

    # invest hives from account to account
    tt.logger.info(f"Start invest hives! @Block: {node.get_last_block_number()}")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __invest,
            wallet,
            authority,
        ),
        args_sequences=(ACCOUNT_NAMES,),
        amount=NUMBER_OF_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    tt.logger.info(f"Finish transfer to vesting! @Block: {node.get_last_block_number()}")

    # delegate rc from initminer to each account
    tt.logger.info(f"Start delegate rc! @Block: {node.get_last_block_number()}")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __delegate_vesting_shares,
            wallet,
            authority,
        ),
        args_sequences=(ACCOUNT_NAMES,),
        amount=NUMBER_OF_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    tt.logger.info(f"Finish delegate rc! @Block: {node.get_last_block_number()}")

    # Unlock delayed votes after vesting delegation.
    node.wait_for_irreversible_block()
    # fixme: after adding ability to pass `--alternate-chain-spec` parameter to node.restart() method
    wallet.close()
    tt.logger.info(f"Unlock delayed votes! @Block: {node.get_last_block_number()}")
    head_block_time = node.get_head_block_time()
    node.close()
    node.run(
        time_control=tt.StartTimeControl(
            start_time=head_block_time + tt.Time.seconds(HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD)
        ),
        alternate_chain_specs=block_log_config,
        arguments=[f"--chain-id={CHAIN_ID}"],
    )
    wallet.run()
    node.wait_number_of_blocks(1)
    # fixme
    tt.logger.info(f"Unlock delayed votes after RC delegation - restart. @Block {node.get_last_block_number()}")

    # create posts
    tt.logger.info(f"Start creating posts! @Block: {node.get_last_block_number()}")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __create_post,
            wallet,
            authority,
        ),
        args_sequences=(ACCOUNT_NAMES[:NUMBER_OF_COMMENTS],),
        amount=NUMBER_OF_COMMENTS,
        chunk_size=ACCOUNTS_PER_CHUNK - 512,
        max_workers=MAX_WORKERS,
    )
    tt.logger.info(f"Finish crateing posts! @Block: {node.get_last_block_number()}")

    with wallet.in_single_transaction():
        for witness in witnesses:
            # change block size to 2mb
            wallet.api.update_witness(
                witness,
                "https://" + witness,
                tt.Account(witness).public_key,
                {
                    # fee is high because the number of `create proposal operations` performed increases the RC costs
                    "account_creation_fee": ACCOUNT_CREATION_FEE_AFTER_HF_20,
                    "maximum_block_size": 2097152,
                    "hbd_interest_rate": 0,
                },
            )

    assert node.api.database.get_hardfork_properties().last_hardfork == 18
    headblock = node.get_last_block_number()
    tt.logger.info(f"Start waiting for current hardfork! @Block: {node.get_last_block_number()}")
    wait_for_current_hardfork(node, current_hardfork_number)
    assert node.api.database.get_hardfork_properties().last_hardfork == current_hardfork_number
    tt.logger.info(f"Finish waiting for current hardfork! @Block: {node.get_last_block_number()}")

    # To avoid a missed block wait +128 blocks ( at least 33% of the blocks should be correct ).
    node.wait_for_block_with_number(headblock + 250)

    # waiting for the block with the last transaction to become irreversible
    node.wait_for_irreversible_block()

    node.close()
    node.block_log.copy_to(block_log_directory)
    tt.BlockLog(block_log_directory, "auto").generate_artifacts()
    tt.logger.info(f"Save block log file to {block_log_directory}")


def __invest(account: str, _) -> list:
    return [["transfer_to_vesting", {"from": account, "to": account, "amount": INVEST_PER_ACCOUNT.as_legacy()}]]


def __delegate_vesting_shares(account: str, _) -> list:
    return [
        [
            "delegate_vesting_shares",
            {"delegatee": f"{account}", "delegator": "initminer", "vesting_shares": DELEGATION_PER_ACCOUNT.as_legacy()},
        ]
    ]


def generate_authority(wallet: tt.Wallet, authority_type: Literal["open_sign", "multi_sign", "single_sign"]) -> dict:
    match authority_type:
        case "open_sign":
            return {
                "owner": {"weight_threshold": 0, "account_auths": [], "key_auths": []},
                "active": {"weight_threshold": 0, "account_auths": [], "key_auths": []},
                "posting": {"weight_threshold": 0, "account_auths": [], "key_auths": []},
                "memo": tt.PublicKey("account", secret="memo"),
            }
        case "multi_sign":
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"owner-{num}") for num in range(3)])
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"active-{num}") for num in range(6)])
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"posting-{num}") for num in range(10)])

            owner_keys = [[tt.PublicKey("account", secret=f"owner-{num}"), 1] for num in range(3)]
            active_keys = [[tt.PublicKey("account", secret=f"active-{num}"), 1] for num in range(6)]
            posting_keys = [[tt.PublicKey("account", secret=f"posting-{num}"), 1] for num in range(10)]
            return {
                "owner": {"weight_threshold": 3, "account_auths": [], "key_auths": owner_keys},
                "active": {"weight_threshold": 6, "account_auths": [], "key_auths": active_keys},
                "posting": {"weight_threshold": 10, "account_auths": [], "key_auths": posting_keys},
                "memo": tt.PublicKey("account", secret="memo"),
            }
        case "single_sign":
            wallet.api.import_key(tt.PrivateKey("account", secret="owner"))
            wallet.api.import_key(tt.PrivateKey("account", secret="active"))
            wallet.api.import_key(tt.PrivateKey("account", secret="posting"))

            return {
                "owner": {
                    "weight_threshold": 1,
                    "account_auths": [],
                    "key_auths": [[tt.PublicKey("account", secret="owner"), 1]],
                },
                "active": {
                    "weight_threshold": 1,
                    "account_auths": [],
                    "key_auths": [[tt.PublicKey("account", secret="active"), 1]],
                },
                "posting": {
                    "weight_threshold": 1,
                    "account_auths": [],
                    "key_auths": [[tt.PublicKey("account", secret="posting"), 1]],
                },
                "memo": tt.PublicKey("account", secret="memo"),
            }


def __create_account_and_fund_hive_and_hbd(account: str, authority: dict) -> list:
    return [
        [
            "account_create",
            {
                "fee": INITIAL_ACCOUNT_CREATION_FEE.as_legacy(),
                "creator": "initminer",
                "new_account_name": account,
                "owner": authority["owner"],
                "active": authority["active"],
                "posting": authority["posting"],
                "memo_key": authority["memo"],
                "json_metadata": "",
            },
        ],
        [
            "transfer",
            {
                "from": "initminer",
                "to": account,
                "amount": HIVE_PER_ACCOUNT - tt.Asset.Test(2).as_legacy(),
                "memo": f"hive_transfer-{account}",
            },
        ],
        [
            "transfer",
            {
                "from": "initminer",
                "to": account,
                "amount": TBD_PER_ACCOUNT.as_legacy(),
                "memo": f"hbd_transfer-{account}",
            },
        ],
    ]


def __create_post(account_name: str, _: None) -> list:
    return [
        [
            "comment",
            {
                "author": account_name,
                "body": generate_random_text(min_length=100, max_length=500),
                "json_metadata": "{}",
                "parent_author": "",
                "parent_permlink": f"category-{account_name}",
                "permlink": f"permlink-{account_name.split('-')[1]}",
                "title": f"title-{account_name}",
            },
        ]
    ]


def __generate_and_broadcast_transaction(
    func: callable, wallet: tt.Wallet, authority: str, account_names: list[str]
) -> None:
    transaction = deepcopy(TRANSACTION_TEMPLATE)

    for name in account_names:
        transaction["operations"].extend(func(name, authority))

    wallet.api.sign_transaction(transaction)

    tt.logger.info(f"Finished: {account_names[-1]}")


def generate_random_text(min_length: int, max_length: int) -> str:
    length_of_text = random.randint(min_length, max_length)
    random_chars = [random_letter() for _ in range(length_of_text)]
    return "".join(random_chars)


def random_letter():
    return chr(random.randrange(97, 97 + 26))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-block-log-directory", type=Path, default=Path(__file__).parent)
    args = parser.parse_args()
    prepare_block_log(args.output_block_log_directory, "open_sign", 1600)
    prepare_block_log(args.output_block_log_directory, "single_sign", 1600)
    prepare_block_log(args.output_block_log_directory, "multi_sign", 2200)
