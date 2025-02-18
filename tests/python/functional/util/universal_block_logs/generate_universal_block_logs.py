from __future__ import annotations

import argparse
import json
import os
import random
from datetime import timedelta
from pathlib import Path
from typing import Final, Literal

from helpy._interfaces.wax import calculate_tapos_data
from loguru import logger

import test_tools as tt
from hive_local_tools.constants import HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD
from hive_local_tools.functional.python import generate_block
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from hive_local_tools.functional.python.operation import get_vesting_price
from schemas.fields.basic import AccountName
from schemas.fields.compound import Authority
from schemas.fields.hive_int import HiveInt
from schemas.operations.account_create_operation import AccountCreateOperationLegacy
from schemas.operations.comment_operation import CommentOperation
from schemas.operations.delegate_vesting_shares_operation import DelegateVestingSharesOperationLegacy
from schemas.operations.transfer_operation import TransferOperationLegacy
from schemas.operations.transfer_to_vesting_operation import TransferToVestingOperationLegacy
from test_tools.__private.wallet.constants import SimpleTransactionLegacy

CHAIN_ID: Final[int] = 24

NUMBER_OF_ACCOUNTS: Final[int] = 2_000_000
NUMBER_OF_COMMENTS: Final[int] = 10_000  # There are active posts from account-0 to account-9999
ACCOUNT_NAMES: list[AccountName] = [AccountName(f"account-{account}") for account in range(NUMBER_OF_ACCOUNTS)]
WITNESSES: Final[list[AccountName]] = [AccountName(f"witness-{w}") for w in range(20)]

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
MAX_WORKERS: Final[int] = os.cpu_count() * 2


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
    node.config.shared_file_size = "48G"
    node.config.plugin.append("queen")
    for witness in ["initminer", *WITNESSES]:
        witness_key = tt.Account(witness).private_key
        if witness != "initminer":
            node.config.witness.append(witness)
            node.config.private_key.append(witness_key)
        save_keys_to_file(name=witness, witness_key=witness_key, file_path=block_log_directory / "witnesses_keys.txt")

    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    five_days_in_blocks = 5 * 24 * 60 * 60 // 3

    block_log_config = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[
            tt.HardforkSchedule(hardfork=18, block_num=0),
            tt.HardforkSchedule(hardfork=current_hardfork_number, block_num=activate_current_hf + five_days_in_blocks),
        ],
        init_supply=INIT_SUPPLY,
        hbd_init_supply=HBD_INIT_SUPPLY,
        initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=INITIAL_VESTING),
        init_witnesses=WITNESSES,
        min_root_comment_interval=3,
    )
    block_log_config.export_to_file(block_log_directory)

    node.run(alternate_chain_specs=block_log_config, arguments=[f"--chain-id={CHAIN_ID}"])
    tt.logger.info(f"Price Hive/Vest: {get_vesting_price(node)}")

    generate_block(node, 1)  # activate hardforks

    wallet = tt.OldWallet(attach_to=node, additional_arguments=[f"--chain-id={CHAIN_ID}"])
    wallet.api.set_transaction_expiration(3600 - 1)

    witnesses = wallet.api.list_witnesses("", 100)
    for witness in witnesses:
        update_witness = wallet.api.update_witness(
            witness,
            "https://" + witness,
            tt.Account(witness).public_key,
            {
                # fee is high because the number of `create proposal operations` performed increases the RC costs
                "account_creation_fee": INITIAL_ACCOUNT_CREATION_FEE,
                "maximum_block_size": 2097152,  # change block size to 2mb
                "hbd_interest_rate": 0,
            },
            broadcast=False,
        )
        node.api.network_broadcast.broadcast_transaction(trx=update_witness)

    tt.logger.info("Wait 43 blocks...")
    generate_block(node, 43)  # wait for the block size to change to 2mb

    authority = generate_authority(wallet, signature_type, output_directory=block_log_directory)

    # create_accounts, fund hbd and hive
    tt.logger.info(f"Start creating accounts! @Block: {node.get_last_block_number()}")
    size = {"open_sign": 1300, "single_sign": 1000, "multi_sign": 500}
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __create_and_fund_account,
            node,
            wallet,
            authority,
        ),
        args_sequences=(ACCOUNT_NAMES,),
        amount=NUMBER_OF_ACCOUNTS,
        chunk_size=size[signature_type],
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info(f"Finish creating accounts! @Block: {node.get_last_block_number()}")

    # invest hives from account to account
    tt.logger.info(f"Start invest hives! @Block: {node.get_last_block_number()}")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __invest,
            node,
            wallet,
            authority,
        ),
        args_sequences=(ACCOUNT_NAMES,),
        amount=NUMBER_OF_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK + 1024,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info(f"Finish transfer to vesting! @Block: {node.get_last_block_number()}")

    # unlock delayed votes after vesting delegation.
    tt.logger.info(f"Unlock delayed votes! @Block: {node.get_last_block_number()}")
    generate_block(node, 1, miss_blocks=HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD // 3)
    tt.logger.info(f"Unlock delayed votes after RC delegation - restart. @Block {node.get_last_block_number()}")

    # create posts
    tt.logger.info(f"Start creating posts! @Block: {node.get_last_block_number()}")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(
            __create_post,
            node,
            wallet,
            authority,
        ),
        args_sequences=(ACCOUNT_NAMES[:NUMBER_OF_COMMENTS],),
        amount=NUMBER_OF_COMMENTS,
        chunk_size=ACCOUNTS_PER_CHUNK - 512,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info(f"Post creation finished! @Block: {node.get_last_block_number()}")

    # vote to increase the fee (aligned with mainnet values)
    for witness in witnesses:
        update_witness = wallet.api.update_witness(
            witness,
            "https://" + witness,
            tt.Account(witness).public_key,
            {
                # fee is high because the number of `create proposal operations` performed increases the RC costs
                "account_creation_fee": ACCOUNT_CREATION_FEE_AFTER_HF_20,
                "maximum_block_size": 2097152,  # change block size to 2mb
                "hbd_interest_rate": 0,
            },
            broadcast=False,
        )
        node.api.network_broadcast.broadcast_transaction(trx=update_witness)
    generate_block(node, 1)

    assert node.api.database.get_hardfork_properties().last_hardfork == 18
    tt.logger.info(f"Start waiting for current hardfork! @Block: {node.get_last_block_number()}")
    while node.api.database.get_hardfork_properties().last_hardfork != current_hardfork_number:
        generate_block(node, 1)
    assert node.api.database.get_hardfork_properties().last_hardfork == current_hardfork_number
    tt.logger.info(f"Finish waiting for current hardfork! @Block: {node.get_last_block_number()}")

    # to avoid a missed block wait +128 blocks ( at least 33% of the blocks should be correct ).
    generate_block(node, 250)

    node.close()
    node.block_log.copy_to(block_log_directory, artifacts="optional")
    timestamp = tt.BlockLog(block_log_directory, "auto").get_head_block_time()
    with open(block_log_directory.joinpath("timestamp"), "w", encoding="utf-8") as file:
        file.write(timestamp.isoformat(timespec="seconds")[:19])
    tt.logger.info(f"Save block log file to {block_log_directory}")


def __invest(account: str, _) -> tuple[TransferToVestingOperationLegacy]:
    return (TransferToVestingOperationLegacy(from_=account, to=account, amount=INVEST_PER_ACCOUNT.as_legacy()),)


def generate_authority(
    wallet: tt.OldWallet,
    authority_type: Literal["open_sign", "multi_sign", "single_sign"],
    output_directory: Path | None = None,
) -> dict:
    def _save(
        auth: dict,
    ) -> None:
        file_path = output_directory / "account_specification.txt"
        file_path.parent.mkdir(parents=True, exist_ok=True)

        with open(file_path, "w", encoding="utf-8") as file:
            file.write(
                f"Authority is the same for {ACCOUNT_NAMES[0]} to {ACCOUNT_NAMES[-1]} - {authority_type}".center(
                    70, "-"
                )
                + "\n\n"
            )
            file.write(json.dumps(auth, default=lambda obj: dict(obj), indent=2))

    match authority_type:
        case "open_sign":
            authority = {
                "owner": Authority(weight_threshold=HiveInt(0), account_auths=[], key_auths=[]),
                "active": Authority(weight_threshold=HiveInt(0), account_auths=[], key_auths=[]),
                "posting": Authority(weight_threshold=HiveInt(0), account_auths=[], key_auths=[]),
                "memo": tt.PublicKey("account", secret="memo"),
            }
            save_keys_to_file(name="account", colony_key=False, file_path=output_directory / "colony_keys.txt")
            _save(authority)
            return authority
        case "multi_sign":
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"secret-{num}") for num in range(5)])
            public_keys = [(tt.PublicKey("account", secret=f"secret-{num}"), HiveInt(1)) for num in range(5)]
            private_keys = [tt.PrivateKey("account", secret=f"secret-{num}") for num in range(5)]

            authority = {
                "owner": Authority(weight_threshold=HiveInt(5), account_auths=[], key_auths=public_keys),
                "active": Authority(weight_threshold=HiveInt(5), account_auths=[], key_auths=public_keys),
                "posting": Authority(weight_threshold=HiveInt(5), account_auths=[], key_auths=public_keys),
                "memo": tt.PublicKey("account", secret="memo"),
            }
            save_keys_to_file(name="account", colony_key=private_keys, file_path=output_directory / "colony_keys.txt")
            _save(authority)
            return authority
        case "single_sign":
            account = tt.Account("account", secret="secret")
            public_key = account.public_key
            private_key = account.private_key
            wallet.api.import_key(private_key)
            weight = HiveInt(1)

            authority = {
                "owner": Authority(weight_threshold=HiveInt(1), account_auths=[], key_auths=[(public_key, weight)]),
                "active": Authority(weight_threshold=HiveInt(1), account_auths=[], key_auths=[(public_key, weight)]),
                "posting": Authority(weight_threshold=HiveInt(1), account_auths=[], key_auths=[(public_key, weight)]),
                "memo": public_key,
            }
            _save(authority)
            save_keys_to_file(name=account.name, colony_key=private_key, file_path=output_directory / "colony_keys.txt")
            return authority


def __create_and_fund_account(
    account: str, authority: dict
) -> tuple[
    AccountCreateOperationLegacy,
    TransferOperationLegacy,
    TransferOperationLegacy,
    DelegateVestingSharesOperationLegacy,
]:
    return (
        AccountCreateOperationLegacy(
            fee=INITIAL_ACCOUNT_CREATION_FEE.as_legacy(),
            creator=AccountName("initminer"),
            new_account_name=AccountName(account),
            owner=authority["owner"],
            active=authority["active"],
            posting=authority["posting"],
            memo_key=authority["memo"],
            json_metadata="",
        ),
        TransferOperationLegacy(
            from_=AccountName("initminer"),
            to=AccountName(account),
            amount=(HIVE_PER_ACCOUNT - tt.Asset.Test(2)).as_legacy(),
            memo=f"hive_transfer-{account}",
        ),
        TransferOperationLegacy(
            from_=AccountName("initminer"),
            to=AccountName(account),
            amount=TBD_PER_ACCOUNT.as_legacy(),
            memo=f"hbd_transfer-{account}",
        ),
        DelegateVestingSharesOperationLegacy(
            delegatee=AccountName(account),
            delegator=AccountName("initminer"),
            vesting_shares=DELEGATION_PER_ACCOUNT.as_legacy(),
        ),
    )


def __create_post(account_name: str, _: None) -> tuple[CommentOperation]:
    return (
        CommentOperation(
            author=AccountName(account_name),
            body=generate_random_text(min_length=100, max_length=500),
            json_metadata="{}",
            parent_author="",
            parent_permlink=f"category-{account_name}",
            permlink=f"permlink-{account_name.split('-')[1]}",
            title=f"title-{account_name}",
        ),
    )


def __generate_and_broadcast_transaction(
    func: callable, node: tt.InitNode, wallet: tt.OldWallet, authority: str, account_names: list[str]
) -> None:
    gdpo = node.api.database.get_dynamic_global_properties()
    block_id = gdpo.head_block_id
    tapos_data = calculate_tapos_data(block_id)
    ref_block_num = tapos_data.ref_block_num
    ref_block_prefix = tapos_data.ref_block_prefix

    assert ref_block_num >= 0, f"ref_block_num value `{ref_block_num}` is invalid`"
    assert ref_block_prefix > 0, f"ref_block_prefix value `{ref_block_prefix}` is invalid`"

    transaction = SimpleTransactionLegacy(
        ref_block_num=HiveInt(ref_block_num),
        ref_block_prefix=HiveInt(ref_block_prefix),
        expiration=gdpo.time + timedelta(seconds=1800),
        extensions=[],
        signatures=[],
        operations=[],
    )

    for name in account_names:
        [transaction.add_operation(op) for op in func(name, authority)]

    sign_transaction = wallet.api.sign_transaction(transaction, broadcast=False)
    with node.temporarily_change_timeout(seconds=120):
        node.api.network_broadcast.broadcast_transaction(trx=sign_transaction)


def generate_random_text(min_length: int, max_length: int) -> str:
    length_of_text = random.randint(min_length, max_length)
    random_chars = [random_letter() for _ in range(length_of_text)]
    return "".join(random_chars)


def random_letter() -> str:
    return chr(random.randrange(97, 97 + 26))


def save_keys_to_file(
    name: str, file_path: Path, colony_key: str | list[str] | None | bool = None, witness_key: str | None = None
) -> None:
    output_directory = file_path.parent
    output_directory.mkdir(parents=True, exist_ok=True)

    content_written = False

    with file_path.open("a", encoding="utf-8") as file:
        if colony_key is False:
            file.write("It's an open authority case, colony will be working without any provided key.\n")
            content_written = True
        elif colony_key:
            if isinstance(colony_key, str):
                colony_key = [colony_key]
            for key in colony_key:
                file.write(f"colony-sign-with = {key}\n")
                content_written = True

        if witness_key:
            file.write(f'witness = "{name}"\nprivate-key = {witness_key}\n')
            content_written = True

    if not content_written:
        file_path.unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-block-log-directory", type=Path, default=Path(__file__).parent)
    args = parser.parse_args()
    prepare_block_log(args.output_block_log_directory, "open_sign", 450)
    prepare_block_log(args.output_block_log_directory, "single_sign", 570)
    prepare_block_log(args.output_block_log_directory, "multi_sign", 1200)
