from __future__ import annotations

import os
from datetime import datetime
from pathlib import Path
from typing import Final

import test_tools as tt
from hive_local_tools.constants import HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD
from hive_local_tools.functional import __generate_and_broadcast_transaction
from hive_local_tools.functional.python import generate_block
from hive_local_tools.functional.python.block_log_generation import parse_block_log_generator_args
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from schemas.fields.assets import AssetHbd
from schemas.fields.assets import AssetHive
from schemas.fields.basic import AccountName, PublicKey
from schemas.fields.compound import Authority
from schemas.operations.account_create_operation import AccountCreateOperation
from schemas.operations.comment_operation import CommentOperation
from schemas.operations.create_proposal_operation import CreateProposalOperation
from schemas.operations.transfer_operation import TransferOperation
from schemas.operations.transfer_to_vesting_operation import TransferToVestingOperation
from schemas.operations.update_proposal_votes_operation import UpdateProposalVotesOperation
from schemas.policies import DisableSwapTypesPolicy, set_policies

NUMBER_OF_VOTING_ACCOUNTS: Final[int] = 12_000
NUMBER_OF_PROPOSALS = 12_000
ACCOUNTS_PER_CHUNK: Final[int] = 1024
MAX_WORKERS: Final[int] = os.cpu_count()
TIME_MULTIPLIER: Final[int] = 4


"""
Disabling type swapping means that the schemas no longer automatically convert simple types into custom types.
This makes the schemas module run much faster, but it can only be used in a few specific cases.
"""
set_policies(DisableSwapTypesPolicy(disabled=True))


def prepare_block_log_with_many_vote_for_proposals(output_block_log_directory: Path) -> None:
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
    output_block_log_directory.mkdir(parents=True, exist_ok=True)

    node = tt.InitNode()
    node.config.shared_file_size = "16G"
    node.config.plugin.append("queen")

    acs = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
    )

    node.run(alternate_chain_specs=acs)
    generate_block(node, 1)  # need to activate hardforks

    wallet = tt.Wallet(attach_to=node)
    wallet.api.set_transaction_expiration(3600 - 1)

    # change block size to 2mb
    update_witness_trx = wallet.api.update_witness(
        "initminer",
        "https://" + "initminer",
        tt.Account("initminer").public_key,
        {
            # fee is high because the number of `create proposal operations` performed increases the RC costs
            "account_creation_fee": tt.Asset.Test(10000),
            "maximum_block_size": 2097152,
            "hbd_interest_rate": 0,
        },
        broadcast=False,
    )
    node.api.network_broadcast.broadcast_transaction(trx=update_witness_trx)

    tt.logger.info("Wait 43 blocks to activate 2mb blocks...")
    generate_block(node, 43)  # activate 2mb blocks

    # create accounts
    tt.logger.info("Started creating voters...")
    voters = [f"voter-{i}" for i in range(NUMBER_OF_VOTING_ACCOUNTS)]
    wallet.api.import_key(tt.PrivateKey("voter"))
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, node, __create_voter),
        args_sequences=(voters,),
        amount=NUMBER_OF_VOTING_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info(f"{len(voters)} accounts successfully created!")

    # fund vests
    tt.logger.info(f"Start fund accounts: {node.get_head_block_time()}, block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, node, __vest_account),
        args_sequences=(voters,),
        amount=NUMBER_OF_VOTING_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info("Finish fund accounts successfully!")

    # transfer HBD to accounts
    tt.logger.info(f"Start transfer accounts: {node.get_head_block_time()}, block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, node, __transfer_account),
        args_sequences=(voters,),
        amount=NUMBER_OF_VOTING_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info("Finish transfer accounts successfully!")

    # wait for new vests to be unlocked ( delayed voting mechanism )
    tt.logger.info(f"Unlock delayed votes! @Block: {node.get_last_block_number()}")
    generate_block(node, 1, miss_blocks=HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD // 3)
    tt.logger.info(f"Unlock delayed votes after RC delegation - restart. @Block {node.get_last_block_number()}")
    next_maintenance_time = tt.Time.parse(node.api.database.get_dynamic_global_properties().next_maintenance_time)

    # create comments
    tt.logger.info(f"Start create comments: {node.get_head_block_time()}, block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, node, __comment_operation),
        args_sequences=(voters,),
        amount=NUMBER_OF_VOTING_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info(f"Finish create comments: {node.get_head_block_time()}, block num: {node.get_last_block_number()}.")

    # create proposals
    tt.logger.info(f"Start create proposals: {node.get_head_block_time()}, block num: {node.get_last_block_number()}.")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(wallet, node, __proposal_operation),
        args_sequences=(voters,),
        amount=NUMBER_OF_VOTING_ACCOUNTS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info(f"Finish create proposals: {node.get_head_block_time()}, block num: {node.get_last_block_number()}.")

    # vote for proposals
    tt.logger.info(
        f"Start voting for proposal: {node.get_head_block_time()}, at block num: {node.get_last_block_number()}."
    )
    execute_function_in_threads(
        __generate_and_broadcast_vote,
        args=(wallet, True, False),
        args_sequences=(voters[1000:11_000],),
        amount=NUMBER_OF_VOTING_ACCOUNTS - 2000,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(node, 1)
    tt.logger.info(
        f"Finish voting for proposal: {node.get_head_block_time()}, at block num: {node.get_last_block_number()}."
    )
    assert node.get_head_block_time() < next_maintenance_time

    tt.logger.info("Generate blocks to maintenance block!")
    generate_blocks_until_date(node, next_maintenance_time)
    assert node.get_head_block_time() > next_maintenance_time

    # delete all votes
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

    # reverse vote for proposals
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
    generate_blocks_until_date(node, next_maintenance_time - tt.Time.seconds(30))
    assert next_maintenance_time > node.get_head_block_time()

    tt.logger.info(f"last block number: {node.get_last_block_number()}")
    head_block_num = node.get_last_block_number()
    timestamp = node.api.block.get_block(block_num=head_block_num).block.timestamp
    tt.logger.info(f"head block timestamp: {timestamp}")

    wallet.close()
    node.close()
    acs.export_to_file(output_block_log_directory)
    node.block_log.copy_to(output_block_log_directory / "votes_on_proposals")


def generate_blocks_until_date(node: tt.InitNode, end_date: datetime) -> None:
    now = node.get_head_block_time()
    seconds_diff = (end_date - now).total_seconds()
    blocks_to_gen = int(seconds_diff // 3) + 1
    generate_block(node, 1, miss_blocks=blocks_to_gen)


def __comment_operation(account: str) -> CommentOperation:
    return CommentOperation(
        author=account,
        body=f"{account}-body",
        json_metadata="{}",
        parent_author="",
        parent_permlink=f"parent-{account}",
        permlink=f"permlink-{account}",
        title=f"{account}-title",
    )


def __create_voter(voter: str) -> AccountCreateOperation:
    key = tt.PublicKey("voter")
    return AccountCreateOperation(
        creator=AccountName("initminer"),
        new_account_name=AccountName(voter),
        json_metadata="{}",
        fee=tt.Asset.Test(10000),
        owner=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        active=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        posting=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        memo_key=PublicKey(key),
    )


def __generate_and_broadcast_vote(wallet: tt.Wallet, approve: bool, reverse: bool, account_names: list[str]) -> None:
    def __generate_vote_for_proposal_operation(account: str, reversed_voting: bool) -> list:
        account_id = int(account.split("-")[1])
        operations = []

        queue = range(0, 1000, 5)
        reverse_queue = range(NUMBER_OF_PROPOSALS - account_id - 1000, NUMBER_OF_PROPOSALS - account_id, 5)

        for proposal_id in reverse_queue if reversed_voting else queue:
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
        wallet.send(operations=operations, broadcast=True, blocking=False)
        operations.clear()


def __proposal_operation(account: str) -> CreateProposalOperation:
    return CreateProposalOperation(
        creator=AccountName(account),
        daily_pay=tt.Asset.Tbd(5),
        receiver=AccountName(account),
        start_date=tt.Time.from_now(seconds=HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD),
        end_date=tt.Time.from_now(seconds=HIVE_GOVERNANCE_VOTE_EXPIRATION_PERIOD + 60 * 60 * 24 * 2),
        subject=f"subject-{account}",
        permlink=f"permlink-{account}",
    )


def __transfer_account(voter: str) -> TransferToVestingOperation:
    return TransferOperation(
        from_="initminer",
        to=voter,
        amount=AssetHbd(amount=10000),
        memo=f"supply_hbd_transfer-{voter}",
    )


def __vest_account(voter: str) -> TransferToVestingOperation:
    account_number = int(voter.split("-")[1])
    return TransferToVestingOperation(from_="initminer", to=voter, amount=AssetHive(amount=(100000 + account_number)))


if __name__ == "__main__":
    args = parse_block_log_generator_args()
    prepare_block_log_with_many_vote_for_proposals(args.output_block_log_directory)
