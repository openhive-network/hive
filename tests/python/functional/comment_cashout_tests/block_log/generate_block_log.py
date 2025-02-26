from __future__ import annotations

import argparse
import os
from datetime import timedelta
from pathlib import Path
from typing import Callable, Final

import shared_tools.networks_architecture as networks
import test_tools as tt
from hive_local_tools.functional.python import generate_block
from hive_local_tools.functional.python.datagen.recurrent_transfer import execute_function_in_threads
from schemas.fields.assets import AssetHiveHF26
from schemas.fields.basic import AccountName, PublicKey
from schemas.fields.compound import Authority
from schemas.fields.hive_int import HiveInt
from schemas.operations.account_create_operation import AccountCreateOperation
from schemas.operations.transfer_to_vesting_operation import TransferToVestingOperation
from schemas.operations.vote_operation import VoteOperation
from shared_tools.complex_networks import generate_networks
from test_tools.__private.wallet.constants import SimpleTransaction
from wax import get_tapos_data
from wax._private.result_tools import to_cpp_string

AMOUNT_OF_ALL_COMMENTS: Final[int] = 60
AMOUNT_OF_ALL_VOTERS: Final[int] = 50_000
ACCOUNTS_PER_CHUNK: Final[int] = 512
MAX_WORKERS: Final[int] = os.cpu_count() * 4

CONFIG = {
    "networks": [
        {"InitNode": True, "FullApiNode": 4, "WitnessNodes": [1, 1, 1]},
    ]
}


def prepare_blocklog_network() -> None:
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
    generate_networks(architecture, Path("base_network_block_log"))


def prepare_blocklog_with_comments_and_votes(output_block_log_directory: Path) -> None:
    """
    1) Function continues creation of block_log with network dependencies (from function `prepare_blocklog_network`)
    2) Comments are created (AMOUNT_OF_ALL_COMMENTS)
    3) Accounts are created that vote for comments (AMOUNT_OF_ALL_VOTERS)
    4) Voting takes place (each account casts a vote). The time from the beginning to the end of voting does not exceed
       one hour.
    5) To get all the votes at the required cashout time ( 1h ), you need to slow down node "x0.5"
    """
    architecture = networks.NetworksArchitecture(False)
    architecture.load(CONFIG)

    tt.logger.info(architecture)

    init_node = tt.InitNode()
    init_node.config.shared_file_size = "1G"
    init_node.config.plugin.append("queen")
    init_node.config.private_key.append("5JcCHFFWPW2DryUFDVd7ZXVj2Zo67rqMcvcq5inygZGBAPR1JoR")
    init_node.config.private_key.append("5JrikNWn1kNyF9fb7ep55Bs9nAqXH15d14G37DSbogyGWoTJyQb")
    init_node.config.private_key.append("5JD9oWa9mzPeGWCXrw7wsbp4v72BWAqHLEdhS3u5AkRLYW7CErL")

    acs = tt.AlternateChainSpecs.parse_file(
        Path(__file__).parent / "base_network_block_log" / "alternate-chain-spec.json"
    )

    init_node.run(
        replay_from=tt.BlockLog(Path(__file__).parent / "base_network_block_log", "auto"),
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
        args=(init_wallet, init_node, __create_voter, None),
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
        args=(init_wallet, init_node, __fund_voter, None),
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

    tt.logger.info(f"Started voting at: {init_node.get_head_block_time()}")
    for comment_number in range(AMOUNT_OF_ALL_COMMENTS):
        tt.logger.info(f"Started voting for comment: {comment_number}")
        execute_function_in_threads(
            __generate_and_broadcast_transaction,
            args=(init_wallet, init_node, __vote_for_comment, comment_number),
            args_sequences=(voters,),
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

    acs.export_to_file(output_block_log_directory)
    init_node.block_log.copy_to(output_block_log_directory)
    init_wallet.close()
    init_node.close()


def wait_for_comment_payment(node: tt.InitNode) -> None:
    cashout_time = tt.Time.parse(
        node.api.database.find_comments(comments=[["creator-0", "post-creator-0"]]).comments[0].cashout_time
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


def __generate_and_broadcast_transaction(
    wallet: tt.Wallet, node: tt.InitNode, func: Callable, comment_number: int | None, account_names: list[str]
) -> None:
    gdpo = node.api.database.get_dynamic_global_properties()
    block_id = gdpo.head_block_id
    tapos_data = get_tapos_data(to_cpp_string(block_id))
    ref_block_num = tapos_data.ref_block_num
    ref_block_prefix = tapos_data.ref_block_prefix

    assert ref_block_num >= 0, f"ref_block_num value `{ref_block_num}` is invalid`"
    assert ref_block_prefix > 0, f"ref_block_prefix value `{ref_block_prefix}` is invalid`"

    transaction = SimpleTransaction(
        ref_block_num=HiveInt(ref_block_num),
        ref_block_prefix=HiveInt(ref_block_prefix),
        expiration=gdpo.time + timedelta(seconds=1800),
        extensions=[],
        signatures=[],
        operations=[],
    )

    for account_number, name in enumerate(account_names):
        if comment_number is not None:
            if account_number == 0 and comment_number == 0:
                # The account listed first in the transaction bears the transaction cost. For this reason,
                # accounts that incur transaction costs have delegated RCs.
                trx = wallet.api.delegate_rc("initminer", [name], 40_000_000_000, broadcast=False)
                node.api.network_broadcast.broadcast_transaction(trx=trx)
                generate_block(node, 1)
            transaction.add_operation(func(name, creator_number=comment_number))  # vote for comment
        else:
            transaction.add_operation(func(name))
    sign_transaction = wallet.api.sign_transaction(transaction, broadcast=False)
    node.api.network_broadcast.broadcast_transaction(trx=sign_transaction)

    tt.logger.info(f"Finished: {account_names[-1]}")


def __create_voter(voter: str) -> AccountCreateOperation:
    key = tt.PublicKey("voter")
    return AccountCreateOperation(
        creator=AccountName("initminer"),
        new_account_name=AccountName(voter),
        json_metadata="{}",
        fee=AssetHiveHF26(amount=3000),
        owner=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        active=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        posting=Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]]),
        memo_key=PublicKey(key),
    )


def __fund_voter(voter: str) -> TransferToVestingOperation:
    return TransferToVestingOperation(from_="initminer", to=voter, amount=AssetHiveHF26(amount=10))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-block-log-directory", type=Path, default=Path(__file__).parent)
    args = parser.parse_args()
    # Step 1 generate base network with witnesses
    os.environ["GENERATE_NEW_BLOCK_LOG"] = "1"
    prepare_blocklog_network()
    # Step 2 generate accounts, comments and votes
    os.environ.pop("GENERATE_NEW_BLOCK_LOG")
    prepare_blocklog_with_comments_and_votes(args.output_block_log_directory)
