from __future__ import annotations

import random
from copy import deepcopy
from pathlib import Path
from typing import Final, Literal

import test_tools as tt
from hive_local_tools import create_alternate_chain_spec_file
from hive_local_tools.constants import (
    ALTERNATE_CHAIN_JSON_FILENAME,
    HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD,
    TRANSACTION_TEMPLATE,
)
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from hive_local_tools.functional.python.operation import get_vesting_price

NUMBER_OF_ACCOUNTS: Final[int] = 100_000
ACCOUNTS_PER_CHUNK: Final[int] = 1024
MAX_WORKERS: Final[int] = 6
WITNESSES = [f"witness-{w}" for w in range(20)]


def prepare_block_log(signature_type: Literal["open_sign", "multi_sign", "single_sign"]) -> None:
    """
    This script generate block_log with specific conditions:
      1) start blockchain with hardfork: 18 ( before introduction RC ),
      2) set and stabilize price hive to vest ( ~1800 vest per 1 hive ),
      3) activate 21 witnesses (witnesses-0 - witnesses-19 + initminer),
      4) change block size to maximum value 2mb,
      5) create accounts - the number is specified in a constant - NUMBER_OF_ACCOUNTS,
      6) fund accounts - each account is credited with HIVE and HBD ( from initminer ),
      7) each account do transfer_to_vesting,
      8) initminer delegate vesting shares to each account ( vests from price stabilization ),
      9) each account create post
      10) wait for activate current hardfork,
      11) restart node unlock `delayed votes` after vesting delegation.
    """
    block_log_directory = Path(__file__).parent / f"block_log_{signature_type}"

    node = tt.InitNode()
    node.config.shared_file_size = "3G"
    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])
    create_alternate_chain_spec_file(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[
            {"hardfork": 18, "block_num": 0},
            {"hardfork": current_hardfork_number, "block_num": 300},
        ],
        init_supply=400_000_000_000,
        hbd_init_supply=30_000_000_000,
        initial_vesting={"hive_amount": 50_000_000_000, "vests_per_hive": 1800},
        init_witnesses=WITNESSES,
        path_to_save=block_log_directory,
    )

    node.run(arguments=["--alternate-chain-spec", str(block_log_directory / ALTERNATE_CHAIN_JSON_FILENAME)])
    tt.logger.info(f"Price Hive / Vest:  {get_vesting_price(node)}")

    wallet = tt.Wallet(attach_to=node)
    wallet.api.set_transaction_expiration(3600 - 1)

    for witness in wallet.api.list_witnesses("", 100):
        # change block size to 2mb
        wallet.api.update_witness(
            witness,
            "https://" + witness,
            tt.Account(witness).public_key,
            {
                # fee is high because the number of `create proposal operations` performed increases the RC costs
                "account_creation_fee": tt.Asset.Test(3),
                "maximum_block_size": 2097152,
                "hbd_interest_rate": 0,
            },
        )

    tt.logger.info("Wait 43 blocks...")
    node.wait_number_of_blocks(43)  # wait for the block size to change to 2mb

    account_names = [f"account-{account}" for account in range(NUMBER_OF_ACCOUNTS)]

    authority = generate_authority(wallet, signature_type)

    # create_accounts, fund hbd and hive
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __create_account_and_fund_hive_and_hbd,
            wallet,
            authority,
        ),
        args_sequences=(account_names,),
        amount=NUMBER_OF_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK - 512,
        max_workers=MAX_WORKERS,
    )
    tt.logger.info(f"{len(account_names)} accounts successfully created!")

    # invest hives from account to account
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __invest,
            wallet,
            authority,
        ),
        args_sequences=(account_names,),
        amount=NUMBER_OF_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    tt.logger.info(f"{len(account_names)} transfer to vesting completed")

    # delegate rc from initminer to each account
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __delegate_vesting_shares,
            wallet,
            authority,
        ),
        args_sequences=(account_names,),
        amount=NUMBER_OF_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    tt.logger.info(f"{len(account_names)} delegate rc completed")

    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __create_post,
            wallet,
            authority,
        ),
        args_sequences=(account_names,),
        amount=NUMBER_OF_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK - 512,
        max_workers=MAX_WORKERS,
    )

    assert node.api.database.get_hardfork_properties().last_hardfork == 18
    wait_for_current_hardfork(node, current_hardfork_number)
    assert node.api.database.get_hardfork_properties().last_hardfork == current_hardfork_number

    # Unlock delayed votes after vesting delegation.
    node.wait_for_irreversible_block()
    node.restart(time_offset=f"+{HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD!s}s")
    tt.logger.info("Unlock delayed votes after RC delegation - restart")

    # waiting for the block with the last transaction to become irreversible
    node.wait_for_irreversible_block()

    head_block_num = node.get_last_block_number()
    timestamp = tt.Time.serialize(node.api.block.get_block(block_num=head_block_num).block.timestamp)
    tt.logger.info(f"head block timestamp: {timestamp}")

    block_log_directory = Path(__file__).parent / f"block_log_{signature_type}"

    with open(block_log_directory / "timestamp", "w", encoding="utf-8") as file:
        file.write(f"{timestamp}")

    node.close()
    node.block_log.copy_to(Path(__file__).parent / f"block_log_{signature_type}")


def __invest(account: str, _) -> list:
    return [["transfer_to_vesting", {"from": account, "to": account, "amount": tt.Asset.Test(1500).as_legacy()}]]


def __delegate_vesting_shares(account: str, _) -> list:
    return [
        [
            "delegate_vesting_shares",
            {"delegatee": f"{account}", "delegator": "initminer", "vesting_shares": tt.Asset.Vest(800_000).as_legacy()},
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
                "fee": tt.Asset.Test(3).as_legacy(),
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
                "amount": tt.Asset.Test(3495).as_legacy(),
                "memo": f"hive_transfer-{account}",
            },
        ],
        [
            "transfer",
            {
                "from": "initminer",
                "to": account,
                "amount": tt.Asset.Tbd(295).as_legacy(),
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
                "permlink": f"permlink-{account_name}",
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


def wait_for_current_hardfork(node, hardfork_version: int) -> None:
    def is_current_hardfork() -> bool:
        return bool(node.api.database.get_hardfork_properties().last_hardfork == hardfork_version)

    tt.logger.info("Wait for activate hardfork...")
    tt.Time.wait_for(is_current_hardfork)
    tt.logger.info("Finish waiting...")


def generate_random_text(min_length: int, max_length: int):
    length_of_text = random.randint(min_length, max_length)
    output = ""
    for _it in range(length_of_text):
        output += random_letter()
    return output


def random_letter():
    return chr(random.randrange(97, 97 + 26))


if __name__ == "__main__":
    prepare_block_log("multi_sign")
    prepare_block_log("open_sign")
    prepare_block_log("single_sign")
