from __future__ import annotations

import os
from pathlib import Path
from typing import Final

import test_tools as tt
from hive_local_tools.constants import HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD
from hive_local_tools.functional.python.datagen.recalculation_proposal_vote_tests import wait_for_maintenance_block
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from hive_local_tools.functional.python.operation import create_transaction_with_any_operation
from schemas.fields.assets.hbd import AssetHbdHF26
from schemas.fields.assets.hive import AssetHiveHF26
from schemas.fields.basic import AccountName
from schemas.operations.comment_operation import CommentOperation
from schemas.operations.create_proposal_operation import CreateProposalOperation
from schemas.operations.transfer_operation import TransferOperation
from schemas.operations.transfer_to_vesting_operation import TransferToVestingOperation
from schemas.operations.update_proposal_votes_operation import UpdateProposalVotesOperation

NUMBER_OF_VOTING_ACCOUNTS: Final[int] = 12_000
NUMBER_OF_PROPOSALS = 12_000
ACCOUNTS_PER_CHUNK: Final[int] = 1024
MAX_WORKERS: Final[int] = os.cpu_count()
TIME_MULTIPLIER: Final[int] = 4


def prepare_block_log_with_many_vote_for_proposals() -> None:
    """
    This script generate block_log with specific conditions:
      1) 12_000 accounts from account-0 to account-11999,
      2) create accounts - the number is specified in a constant - NUMBER_OF_VOTING_ACCOUNTS,
      3) create 12_000 comments,
      4) create 12_000 proposals,
      5) vote for proposals - each account vote for 1000 proposals, eg:
          - account-0 vote for proposals from proposal-0 to proposal-999,
          - account-1 vote for proposals from proposal-1 to proposal-1000,
          - account-2 vote for proposals from proposal-2 to proposal-1001 etc.
      6) wait for maintenance block,
      7) delete all votes for proposal,
      8) reverse vote for proposals eg:
         - account-0 vote for proposals from proposal-11000 to proposal-11999,
         - account-1 vote for proposals from proposal-10999 to proposal-11998,
         - account-2 vote for proposals from proposal-10998 to proposal-11997 etc.
      9) Save block_log  30s before maintenance block.
    """
    node = tt.InitNode()
    node.config.shared_file_size = "16G"
    node.run(time_control=tt.SpeedUpRateTimeControl(speed_up_rate=TIME_MULTIPLIER))

    wallet = tt.Wallet(attach_to=node)
    wallet.api.set_transaction_expiration(3600 - 1)

    # change block size to 2mb
    wallet.api.update_witness(
        "initminer",
        "https://" + "initminer",
        tt.Account("initminer").public_key,
        {
            # fee is high because the number of `create proposal operations` performed increases the RC costs
            "account_creation_fee": tt.Asset.Test(10000),
            "maximum_block_size": 2097152,
            "hbd_interest_rate": 0,
        },
    )

    tt.logger.info("Wait 43 blocks...")
    node.wait_number_of_blocks(43)  # wait for the block size to change to 2mb

    voters = [voter.name for voter in wallet.create_accounts(NUMBER_OF_VOTING_ACCOUNTS)]
    tt.logger.info(f"{len(voters)} accounts successfully created!")

    # Fund accounts
    tt.logger.info(f"Start fund accounts: {node.get_head_block_time()}, block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_fund,
        args=(wallet,),
        args_sequences=(voters,),
        amount=NUMBER_OF_VOTING_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    # Wait for new vests to be unlocked ( delayed voting mechanism )
    node.wait_for_irreversible_block()
    node.restart(
        time_control=tt.StartTimeControl(
            start_time=node.get_head_block_time() + tt.Time.seconds(HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD),
            speed_up_rate=TIME_MULTIPLIER,
        )
    )
    next_maintenance_time = tt.Time.parse(node.api.database.get_dynamic_global_properties().next_maintenance_time)
    wallet.api.set_transaction_expiration(3600 - 1)

    # Create comments
    tt.logger.info(f"Start create comments: {node.get_head_block_time()}, block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_comment,
        args=(wallet,),
        args_sequences=(voters,),
        amount=NUMBER_OF_VOTING_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )

    # Create proposals
    tt.logger.info(f"Start create proposals: {node.get_head_block_time()}, block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_proposal,
        args=(wallet,),
        args_sequences=(voters,),
        amount=NUMBER_OF_VOTING_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )

    # Vote for proposals
    tt.logger.info(f"Start voting: {node.get_head_block_time()}, at block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_vote,
        args=(wallet, True, False),
        args_sequences=(voters[1000:11_000],),
        amount=NUMBER_OF_VOTING_ACCOUNTS - 2000,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    assert node.get_head_block_time() < next_maintenance_time

    wait_for_maintenance_block(node, next_maintenance_time)
    assert node.get_head_block_time() > next_maintenance_time

    # Delete all votes
    tt.logger.info(f"Start delete votes: {node.get_head_block_time()}, at block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_vote,
        args=(wallet, False, False),
        args_sequences=(voters[1000:11_000],),
        amount=NUMBER_OF_VOTING_ACCOUNTS - 2000,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )

    assert (
        len(
            node.api.database.list_proposal_votes(
                start=[""], limit=1000, order="by_voter_proposal", order_direction="ascending", status="all"
            ).proposal_votes
        )
        == 0
    ), "Votes for proposals were not removed correctly."
    tt.logger.info(f"Headblock time before reverse voting: {node.get_head_block_time()}")
    next_maintenance_time = tt.Time.parse(node.api.database.get_dynamic_global_properties().next_maintenance_time)

    # Reverse vote for proposals
    tt.logger.info(f"Remaining time for reverse voting: {next_maintenance_time - node.get_head_block_time()}")
    tt.logger.info(f"Start reverse voting: {node.get_head_block_time()}, at block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_vote,
        args=(wallet, True, True),
        args_sequences=(voters[1000:11_000],),
        amount=NUMBER_OF_VOTING_ACCOUNTS - 2000,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )

    assert next_maintenance_time > node.get_head_block_time(), "Maintenance time exceeded"
    tt.logger.info(f"next_maintenance_time after all: {next_maintenance_time}, hbn: {node.get_last_block_number()}")
    wait_for_maintenance_block(node, next_maintenance_time - tt.Time.seconds(30))
    assert next_maintenance_time > node.get_head_block_time()

    # waiting for the block with the last transaction to become irreversible
    node.wait_for_irreversible_block()

    tt.logger.info(f"last block number: {node.get_last_block_number()}")

    head_block_num = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num=head_block_num).block.timestamp
    tt.logger.info(f"head block timestamp: {timestamp}")

    wallet.close()
    node.close()
    node.block_log.copy_to(Path(__file__).parent)


def __generate_and_broadcast_fund(wallet: tt.Wallet, account_names: list[str]) -> None:
    def __generate_operations_to_fund_account(account: str) -> list:
        account_number = int(account.split("-")[1])
        return [
            TransferToVestingOperation(
                from_="initminer", to=account, amount=AssetHiveHF26(amount=(100000 + account_number))
            ),
            TransferOperation(
                from_="initminer",
                to=account,
                amount=AssetHbdHF26(amount=10000),
                memo=f"supply_hbd_transfer-{account}",
            ),
        ]

    operations = []
    for name in account_names:
        operations.extend(__generate_operations_to_fund_account(name))

    create_transaction_with_any_operation(wallet, operations, broadcast=True)
    tt.logger.info(f"Finished fund: {account_names[-1]}")


def __generate_and_broadcast_comment(wallet: tt.Wallet, account_names: list[str]) -> None:
    def __generate_comment_operation(account: str) -> CommentOperation:
        return CommentOperation(
            author=account,
            body=f"{account}-body",
            json_metadata="{}",
            parent_author="",
            parent_permlink=f"parent-{account}",
            permlink=f"permlink-{account}",
            title=f"{account}-title",
        )

    operations = []
    for name in account_names:
        operations.append(__generate_comment_operation(name))

    create_transaction_with_any_operation(wallet, operations)
    tt.logger.info(f"Finished comment: {account_names[-1]}")


def __generate_and_broadcast_proposal(wallet: tt.Wallet, account_names: list[str]) -> None:
    def __generate_proposal_operation(account: str) -> CreateProposalOperation:
        return CreateProposalOperation(
            creator=AccountName(account),
            daily_pay=tt.Asset.Tbd(5),
            receiver=AccountName(account),
            start_date=tt.Time.from_now(seconds=HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD),
            end_date=tt.Time.from_now(seconds=HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD + 60 * 60 * 24 * 2),
            subject=f"subject-{account}",
            permlink=f"permlink-{account}",
        )

    operations = []
    for name in account_names:
        operations.append(__generate_proposal_operation(name))

    create_transaction_with_any_operation(wallet, operations, broadcast=True)
    tt.logger.info(f"Finished proposal: {account_names[-1]}")


def __generate_and_broadcast_vote(wallet: tt.Wallet, approve: bool, reverse: bool, account_names: list[str]) -> None:
    def __generate_vote_for_proposal_operation(account: str, reversed_voting: bool) -> list:
        account_id = int(account.split("-")[1])
        operations = []

        rev = range(NUMBER_OF_PROPOSALS - account_id - 1000, NUMBER_OF_PROPOSALS - account_id, 5)

        for proposal_id in rev if reversed_voting else range(0, 1000, 5):
            operations.append(
                UpdateProposalVotesOperation(
                    proposal_ids=(
                        list(range(proposal_id, proposal_id + 5))
                        if reversed_voting
                        else list(range(account_id + proposal_id, account_id + proposal_id + 5))
                    ),
                    approve=approve,
                    voter=AccountName(account),
                )
            )

        return operations

    full_list = account_names
    chunk_size = int(len(full_list) / 8)
    divided_lists = [full_list[i * chunk_size : (i + 1) * chunk_size] for i in range(8)]

    operations = []
    for account_0, account_1, account_2, account_3, account_4, account_5, account_6, account_7 in zip(*divided_lists):
        for acc in [account_0, account_1, account_2, account_3, account_4, account_5, account_6, account_7]:
            operations.extend(__generate_vote_for_proposal_operation(acc, reverse))
            tt.logger.info(f"Finished {'reverse vote' if reverse else 'vote'} for proposal: {acc}")

        create_transaction_with_any_operation(wallet, operations, broadcast=True)
        operations.clear()


if __name__ == "__main__":
    prepare_block_log_with_many_vote_for_proposals()
