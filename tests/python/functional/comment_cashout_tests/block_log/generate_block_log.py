from __future__ import annotations

import os
from pathlib import Path
from typing import Final

import shared_tools.networks_architecture as networks
import test_tools as tt
from hive_local_tools.functional import __generate_and_broadcast_transaction
from hive_local_tools.functional.python import generate_block
from hive_local_tools.functional.python.block_log_generation import parse_block_log_generator_args
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from schemas.fields.assets import AssetHive
from schemas.fields.basic import AccountName, PublicKey
from schemas.fields.compound import Authority
from schemas.operations.account_create_operation import AccountCreateOperation
from schemas.operations.transfer_to_vesting_operation import TransferToVestingOperation
from schemas.operations.vote_operation import VoteOperation
from schemas.policies import DisableSwapTypesPolicy, set_policies
from shared_tools.complex_networks import generate_networks

AMOUNT_OF_ALL_COMMENTS: Final[int] = 60
AMOUNT_OF_ALL_VOTERS: Final[int] = 50_000
ACCOUNTS_PER_CHUNK: Final[int] = 512
MAX_WORKERS: Final[int] = os.cpu_count() * 4

CONFIG = {
    "networks": [
        {"InitNode": True, "FullApiNode": 4, "WitnessNodes": [1, 1, 1]},
    ]
}

"""
Disabling type swapping means that the schemas no longer automatically convert simple types into custom types.
This makes the schemas module run much faster, but it can only be used in a few specific cases.
"""
set_policies(DisableSwapTypesPolicy(disabled=True))


def prepare_blocklog_network(output_block_log_directory: Path) -> None:
    """
    Creating a block log with a network based on the configuration.
    In one network:
    - 1 InitNode,
    - 4 FullApiNodes,
    - 3 WitnessNodes (each with one witness).
    """
    architecture = networks.NetworksArchitecture()
    architecture.load(CONFIG)

    tt.logger.info(architecture)
    Path(output_block_log_directory / "base_network_block_log").mkdir(parents=True, exist_ok=True)
    generate_networks(architecture, output_block_log_directory / "base_network_block_log", terminate_nodes=True)
    tt.logger.info(f"Save block log file to {output_block_log_directory/'base_network_block_log'}")


def prepare_blocklog_with_comments_and_votes(output_block_log_directory: Path) -> None:
    """
    1) Function continues creation of block_log with network dependencies (from function `prepare_blocklog_network`)
    2) Comments are created (AMOUNT_OF_ALL_COMMENTS)
    3) Accounts are created that vote for comments (AMOUNT_OF_ALL_VOTERS)
    4) Voting takes place (each account casts a vote). The time from the beginning to the end of voting does not exceed
       one hour.
    5) To get all the votes at the required cashout time ( 1h ), you need to slow down node "x0.5"
    6) Force start on CI
    """
    output_block_log_directory.mkdir(parents=True, exist_ok=True)

    architecture = networks.NetworksArchitecture(False)
    architecture.load(CONFIG)

    tt.logger.info(architecture)

    init_node = tt.InitNode()
    init_node.config.shared_file_size = "1G"
    init_node.config.plugin.append("queen")

    # private-keys to witnesses [witness0-alpha, witness1-alpha, witness2-alpha]
    init_node.config.private_key.append("5JcCHFFWPW2DryUFDVd7ZXVj2Zo67rqMcvcq5inygZGBAPR1JoR")
    init_node.config.private_key.append("5JrikNWn1kNyF9fb7ep55Bs9nAqXH15d14G37DSbogyGWoTJyQb")
    init_node.config.private_key.append("5JD9oWa9mzPeGWCXrw7wsbp4v72BWAqHLEdhS3u5AkRLYW7CErL")

    # slot 0 of block log
    base_network_block_lo_genesis_time = tt.BlockLog(
        output_block_log_directory / "base_network_block_log", "auto"
    ).get_block(block_number=1).timestamp - tt.Time.seconds(3)
    acs = tt.AlternateChainSpecs(
        genesis_time=int(base_network_block_lo_genesis_time.timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=28, block_num=1)],
        hbd_init_supply=100_000,
        init_supply=200_000_000_000,
        initial_vesting=tt.InitialVesting(hive_amount=10_000_000_000, vests_per_hive=1800),
        witness_custom_op_block_limit=100,
    )

    init_node.run(
        replay_from=tt.BlockLog(output_block_log_directory / "base_network_block_log", "auto"),
        alternate_chain_specs=acs,
    )

    init_wallet = tt.Wallet(attach_to=init_node)
    init_wallet.api.set_transaction_expiration(1000)

    init_wallet.api.import_key(tt.Account("voter").private_key)
    witnesses = init_wallet.api.list_witnesses("", 100, only_names=True)

    # change block size to 2 mb
    for witness_name in witnesses:
        trx = init_wallet.api.update_witness(
            witness_name,
            "http://url.html",
            tt.Account(witness_name).public_key,
            {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 2097152, "hbd_interest_rate": 1000},
            broadcast=False,
        )
        init_node.api.network_broadcast.broadcast_transaction(trx=trx)

    generate_block(init_node, 43)  # wait for the block size to change to 2mb
    tt.logger.info("Wait 43 blocks...")

    tt.logger.info("Started creating voters...")
    voters = [f"voter-{i}" for i in range(AMOUNT_OF_ALL_VOTERS)]
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(init_wallet, init_node, __create_voter),
        args_sequences=(voters,),
        amount=AMOUNT_OF_ALL_VOTERS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(init_node, 1)
    tt.logger.info("Finished creating voters.")

    tt.logger.info("Started funding voters...")
    execute_function_in_threads(
        __generate_and_broadcast_transaction,
        args=(init_wallet, init_node, __fund_voter),
        args_sequences=(voters,),
        amount=AMOUNT_OF_ALL_VOTERS,
        chunk_size=ACCOUNTS_PER_CHUNK,
        max_workers=MAX_WORKERS,
    )
    generate_block(init_node, 1)
    tt.logger.info("Finished funding voters.")

    tt.logger.info("Creating comments started...")
    create_comments(init_node, init_wallet)
    tt.logger.info("Creating comments finished...")

    tt.logger.info("Start Delegate RC...")
    delegate_rc_to_payers(init_node, init_wallet, voters, ACCOUNTS_PER_CHUNK)
    tt.logger.info("Finish Delegate RC...")

    tt.logger.info(f"Started voting at: {init_node.get_head_block_time()}")
    for comment_number in range(AMOUNT_OF_ALL_COMMENTS):
        tt.logger.info(f"Started voting for comment: {comment_number}")
        execute_function_in_threads(
            __generate_and_broadcast_transaction,
            args=(init_wallet, init_node, __vote_for_comment),
            args_sequences=(voters,),
            kwargs={"creator_number": comment_number},
            amount=AMOUNT_OF_ALL_VOTERS,
            chunk_size=ACCOUNTS_PER_CHUNK,
            max_workers=MAX_WORKERS,
        )
        generate_block(init_node, 1)
    tt.logger.info(f"Finished voting at: {init_node.get_head_block_time()}")

    wait_for_comment_payment(init_node)

    timestamp = init_node.api.block.get_block(block_num=init_node.get_last_block_number())["block"]["timestamp"]
    tt.logger.info(f"Final block_log head block number: {init_node.get_last_block_number()}")
    tt.logger.info(f"Final block_log head block timestamp: {timestamp}")

    block_log_directory_comments_and_votes = output_block_log_directory / "comments_and_votes"
    acs.export_to_file(block_log_directory_comments_and_votes)
    init_node.block_log.copy_to(block_log_directory_comments_and_votes)
    init_wallet.close()
    init_node.close()


def wait_for_comment_payment(node: tt.InitNode) -> None:
    cashout_time = tt.Time.parse(
        node.api.database.get_comment_pending_payouts(comments=[["creator-0", "post-creator-0"]])
        .cashout_infos[0]
        .cashout_info.cashout_time
    )
    tt.logger.info(f"Cashout time: {cashout_time}")
    tt.logger.info(f"Head block time: {node.get_head_block_time()}")

    time_diff = ((cashout_time - node.get_head_block_time()) - tt.Time.seconds(60)).seconds
    generate_block(node, time_diff // 3)
    tt.logger.info(f"Finished waiting: {node.get_head_block_time()}")


def create_comments(node: tt.InitNode, wallet: tt.Wallet) -> None:
    creators = [f"creator-{num}" for num in range(AMOUNT_OF_ALL_COMMENTS)]
    for creator in creators:
        transaction = wallet.api.create_account("initminer", creator, "{}", broadcast=False)
        node.api.network_broadcast.broadcast_transaction(trx=transaction)
    generate_block(node, 1)

    for creator in creators:
        transaction = wallet.api.transfer_to_vesting("initminer", creator, tt.Asset.Test(10), broadcast=False)
        node.api.network_broadcast.broadcast_transaction(trx=transaction)
    generate_block(node, 1)

    for creator in creators:
        transaction = wallet.api.post_comment(
            creator,
            f"post-{creator}",
            "",
            f"test-parent-permlink-{creator}",
            f"test-title-{creator}",
            f"test-body-{creator}",
            "{}",
            broadcast=False,
        )
        node.api.network_broadcast.broadcast_transaction(trx=transaction)
    generate_block(node, 1)


def __vote_for_comment(voter: str, creator_number: str) -> VoteOperation:
    return VoteOperation(
        voter=voter,
        author=f"creator-{creator_number}",
        permlink=f"post-creator-{creator_number}",
        weight=int(50 * 10000 / AMOUNT_OF_ALL_COMMENTS),
    )


def __create_voter(voter: str) -> AccountCreateOperation:
    key = tt.PublicKey("voter")
    return AccountCreateOperation(
        creator=AccountName("initminer"),
        new_account_name=AccountName(voter),
        json_metadata="{}",
        fee=AssetHive(amount=3000),
        owner=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        active=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        posting=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        memo_key=PublicKey(key),
    )


def __fund_voter(voter: str) -> TransferToVestingOperation:
    return TransferToVestingOperation(from_="initminer", to=voter, amount=AssetHive(amount=10))


def delegate_rc_to_payers(node: tt.InitNode, wallet: tt.Wallet, voters: list[str], accounts_per_chunks: int) -> None:
    """
    The account listed first in the transaction bears the transaction cost. For this reason,
    accounts that incur transaction costs have delegated RCs.
    """
    chunks = [voters[i : i + accounts_per_chunks] for i in range(0, len(voters), accounts_per_chunks)]

    payers = [payer[0] for payer in chunks]
    transaction = wallet.api.delegate_rc("initminer", payers, 40_000_000_000, broadcast=False)
    node.api.network_broadcast.broadcast_transaction(trx=transaction)
    generate_block(node, 1)


if __name__ == "__main__":
    args = parse_block_log_generator_args()
    # Step 1 generate base network with witnesses
    os.environ["GENERATE_NEW_BLOCK_LOG"] = "1"
    prepare_blocklog_network(args.output_block_log_directory)
    # Step 2 generate accounts, comments and votes
    os.environ.pop("GENERATE_NEW_BLOCK_LOG")
    prepare_blocklog_with_comments_and_votes(args.output_block_log_directory)
