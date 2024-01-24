from __future__ import annotations

from concurrent.futures import ProcessPoolExecutor, as_completed, wait
from functools import partial
from pathlib import Path
from typing import Final, Literal

from schemas.operations.representations import HF26Representation
from generate_operations import prepare_operations_for_transactions

import test_tools as tt
from hive_local_tools import ALTERNATE_CHAIN_JSON_FILENAME
from hive_local_tools.functional.python.operation import get_vesting_price

import wax
from beekeeper import Beekeeper
from generate_transaction_template import (
    generate_transaction_template,
    SimpleTransaction,
)


NUMBER_OF_ACCOUNTS: Final[int] = 100_000
ACCOUNTS_PER_CHUNK: Final[int] = 1024
MAX_WORKERS: Final[int] = 6
WITNESSES = [f"witness-{w}" for w in range(20)]
account_names = [f"account-{account}" for account in range(NUMBER_OF_ACCOUNTS)]


def prepare_second_stage_of_block_log(signature_type: Literal["open_sign", "multi_sign", "single_sign"]) -> None:
    block_log_directory = Path(__file__).parent / f"block_log_{signature_type}"
    block_log_path = block_log_directory / "block_log"
    timestamp_path = block_log_directory / "timestamp"
    alternate_chain_spec_path = block_log_directory / ALTERNATE_CHAIN_JSON_FILENAME

    with open(timestamp_path, encoding="utf-8") as file:
        timestamp = tt.Time.parse(file.read())

    node = tt.InitNode()
    node.config.plugin.append("account_history_api")
    node.config.shared_file_size = "24G"
    node.config.webserver_thread_pool_size = "64"
    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    node.run(
        replay_from=block_log_path,
        time_offset=tt.Time.serialize(timestamp, format_=tt.TimeFormats.TIME_OFFSET_FORMAT),
        wait_for_live=True,
        arguments=["--alternate-chain-spec", str(alternate_chain_spec_path)],
    )

    wallet = tt.Wallet(attach_to=node, additional_arguments=["--transaction-serialization=hf26"])
    prepare_wallet(wallet, signature_type)

    assert len(wallet.api.list_witnesses("", 1000)) == 21, "Incorrect number of witnesses"
    assert get_vesting_price(node) > 1_799_990, "Too low price Hive to Vest"

    tt.logger.info("Start Create Operations")
    operations = prepare_operations_for_transactions(
        iterations=1, ops_in_one_element=1, elements_number_for_iteration=12_000
    )
    tt.logger.info("Finish Create Operations")

    # Sign transactions
    tt.logger.info("Start Signing Transactions")
    http_endpoint = node.http_endpoint.as_string()
    ws_endpoint = node.ws_endpoint.as_string()
    # max_sign_workers = 12  # fixme
    max_sign_workers = 4  # fixme
    with ProcessPoolExecutor(max_workers=max_sign_workers) as executor:
        bk = Beekeeper()
        bk.launch()
        wallet = "initial_wallet"
        password = "password"
        bk.api.create(wallet_name=wallet, password=password)

        num_chunks = max_sign_workers
        chunk_size = len(operations) // num_chunks
        chunks = [operations[i : i + chunk_size] for i in range(0, len(operations), chunk_size)]

        futures = []
        for iteration, chunk in enumerate(chunks):
            futures.append(
                executor.submit(
                    sign_chunk_of_transactions_by_beekeeper,
                    chunk,
                    http_endpoint,
                    ws_endpoint,
                    signature_type,
                    iteration,
                )
            )

        for x in futures:
            assert x.exception() is None

        signed_transactions = [result for future in as_completed(futures) for result in future.result()]
        executor.shutdown(cancel_futures=True, wait=False)
    tt.logger.info("Finish Signing Transactions")

    # Broadcasting
    tt.logger.info("Start Broadcasting")
    max_workers = 16
    with ProcessPoolExecutor(max_workers=max_workers) as executor:
        processor = BroadcastTransactionsChunk()

        num_chunks = max_workers
        chunk_size = len(signed_transactions) // num_chunks
        chunks = [signed_transactions[i : i + chunk_size] for i in range(0, len(signed_transactions), chunk_size)]

        single_transaction_broadcast_with_address = partial(
            processor.broadcast_chunk_of_transactions, init_node_address=http_endpoint
        )

        results = []
        results.extend(list(executor.map(single_transaction_broadcast_with_address, chunks)))
        executor.shutdown(cancel_futures=True, wait=False)
    tt.logger.info("Finish Broadcasting")
    ####################################################################################################################

    # todo: część zapisu block_loga
    # assert node.get_last_block_number() >= 57600, "Generated block log its shorted than 2 days."
    # waiting for the block with the last transaction to become irreversible
    # node.wait_for_irreversible_block()
    #
    # head_block_num = node.get_last_block_number()
    # timestamp = tt.Time.serialize(node.api.block.get_block(block_num=head_block_num).block.timestamp)
    # tt.logger.info(f"head block timestamp: {timestamp}")
    #
    # block_log_directory = Path(__file__).parent / f"block_log_second_stage_{signature_type}"
    #
    # with open(block_log_directory / "timestamp", "w", encoding="utf-8") as file:
    #     file.write(f"{timestamp}")
    #
    # node.close()
    # node.block_log.copy_to(Path(__file__).parent / f"block_log_second_stage_{signature_type}")


def sign_chunk_of_transactions_by_beekeeper(
    chunk: list[dict],
    init_node_http_endpoint: str,
    init_node_ws_endpoint: str,
    signature_type: str,
    iteration: int,
) -> list:
    beekeeper = Beekeeper()
    beekeeper.launch()

    wallet_name = f"process_wallet_{iteration}"
    beekeeper.api.create(wallet_name=wallet_name, password="password")
    imported_key = import_keys_to_beekeeper(beekeeper, f"process_wallet_{iteration}", signature_type)

    remote_node = tt.RemoteNode(http_endpoint=init_node_http_endpoint, ws_endpoint=init_node_ws_endpoint)

    gdpo = remote_node.api.database.get_dynamic_global_properties()
    node_config = remote_node.api.database.get_config()

    transactions_in_chunk = []
    for pack_operations in chunk:
        trx: SimpleTransaction = generate_transaction_template(gdpo)
        trx.operations.append(*[HF26Representation(type=op.get_name_with_suffix(), value=op) for op in pack_operations])
        sig_digest = wax.calculate_sig_digest(
            trx.json(by_alias=True).encode("ascii"), node_config.HIVE_CHAIN_ID.encode("ascii")
        ).result.decode("ascii")
        trx.signatures.append(beekeeper.api.sign_digest(sig_digest=sig_digest, public_key=imported_key).signature)
        transactions_in_chunk.append(trx)
    return transactions_in_chunk


def import_keys_to_beekeeper(bk: Beekeeper, wallet_name: str, signature_type: str):
    match signature_type:
        case "open_sign":
            tt.logger.info("open_sign")
        case "single_sign":
            imported_owner = bk.api.import_key(
                wallet_name=wallet_name, wif_key=tt.Account("account", secret="owner").private_key
            ).public_key
            imported_active = bk.api.import_key(
                wallet_name=wallet_name, wif_key=tt.Account("account", secret="active").private_key
            ).public_key
            imported_posting = bk.api.import_key(
                wallet_name=wallet_name, wif_key=tt.Account("account", secret="posting").private_key
            ).public_key
        case "multi_sign":
            [
                bk.api.import_key(
                    wallet_name=wallet_name, wif_key=tt.Account("account", secret=f"owner-{num}").private_key
                ).public_key
                for num in range(3)
            ]
            [
                bk.api.import_key(
                    wallet_name=wallet_name, wif_key=tt.Account("account", secret=f"active-{num}").private_key
                )
                for num in range(6)
            ]
            [
                bk.api.import_key(
                    wallet_name=wallet_name, wif_key=tt.Account("account", secret=f"posting-{num}").private_key
                )
                for num in range(10)
            ]
    return imported_owner


def prepare_wallet(wallet: tt.Wallet, signature_type: str) -> tt.Wallet:
    wallet.api.set_transaction_expiration(3600 - 1)

    match signature_type:
        case "open_sign":
            tt.logger.info("open_sign")
        case "single_sign":
            wallet.api.import_key(tt.PrivateKey("account", secret="owner"))
            wallet.api.import_key(tt.PrivateKey("account", secret="active"))
            wallet.api.import_key(tt.PrivateKey("account", secret="posting"))
        case "multi_sign":
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"owner-{num}") for num in range(3)])
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"active-{num}") for num in range(6)])
            wallet.api.import_keys([tt.PrivateKey("account", secret=f"posting-{num}") for num in range(10)])
    return wallet


class BroadcastTransactionsChunk:
    def broadcast_chunk_of_transactions(self, chunk: list[dict], init_node_address: str) -> None:
        remote_node = tt.RemoteNode(http_endpoint=init_node_address)
        for trx in chunk:
            remote_node.api.network_broadcast.broadcast_transaction(trx=trx)


if __name__ == "__main__":
    # prepare_second_stage_of_block_log("open_sign")
    prepare_second_stage_of_block_log("single_sign")
    # prepare_second_stage_of_block_log("multi_sign")
