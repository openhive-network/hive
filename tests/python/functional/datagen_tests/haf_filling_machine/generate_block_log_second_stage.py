from __future__ import annotations

import json
import tempfile
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
from functools import partial
from pathlib import Path
from typing import TYPE_CHECKING, Any, Final, Literal

import generate_operations

import test_tools as tt
from hive_local_tools import ALTERNATE_CHAIN_JSON_FILENAME
from hive_local_tools.functional.python.operation import create_transaction_with_any_operation, get_vesting_price

if TYPE_CHECKING:
    from schemas.operation import Operation

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
    wallet.api.set_transaction_expiration(3600 - 1)

    assert len(wallet.api.list_witnesses("", 1000)) == 21, "Incorrect number of witnesses"
    assert get_vesting_price(node) > 1_799_990, "Too low price Hive to Vest"

    ####################################################################################################################
    # 1. Lepienie transakcji,
    # 2. sprawdzanie rozmiaru
    # 3. doklejanie
    # 4. wysyłanie

    # todo: SET KEYS IMPORTS
    # single sign imports
    wallet.api.import_key(tt.PrivateKey("account", secret="owner"))
    wallet.api.import_key(tt.PrivateKey("account", secret="active"))
    wallet.api.import_key(tt.PrivateKey("account", secret="posting"))

    # multi sign imports
    # wallet.api.import_keys([tt.PrivateKey("account", secret=f"owner-{num}") for num in range(3)])
    # wallet.api.import_keys([tt.PrivateKey("account", secret=f"active-{num}") for num in range(6)])
    # wallet.api.import_keys([tt.PrivateKey("account", secret=f"posting-{num}") for num in range(10)])

    operations = generate_operations.create_list_of_operations(100, 1)

    # for name in [f"account-{num}" for num in range(10_000)]:
    #     operations.append([create_transfer(name)])

    signed_transactions = __prepare_and_sign_transactions(wallet, operations)

    with tempfile.NamedTemporaryFile(mode="w", delete=False) as tmp:
        for transaction in signed_transactions:
            json.dump(transaction, tmp)
            tmp.write("\n")
        transactions_file_path = tmp.name

    with open(transactions_file_path) as file:
        lines = file.readlines()

    transactions = [json.loads(line.strip()) for line in lines]

    http_endpoint = node.http_endpoint.as_string()
    # __fast_broadcast(node, signed_transactions)
    max_workers = 16
    with ProcessPoolExecutor(max_workers=max_workers) as executor:
        processor = BroadcastSingleTransaction()

        num_chunks = max_workers
        chunk_size = len(transactions) // num_chunks
        chunks = [transactions[i : i + chunk_size] for i in range(0, len(transactions), chunk_size)]

        single_transaction_broadcast_with_address = partial(
            processor.single_transaction_broadcast, node_address=http_endpoint
        )

        results = []
        results.extend(list(executor.map(single_transaction_broadcast_with_address, chunks)))

    print("Final results:", results)
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


class BroadcastSingleTransaction:
    def single_transaction_broadcast(self, chunk: list[dict], node_address: str) -> None:
        node = tt.RemoteNode(http_endpoint=node_address)
        for trx in chunk:
            node.api.network_broadcast.broadcast_transaction(trx=trx)


def __fast_broadcast2(node, chunk):
    for transaction in chunk:
        # node.api.wallet_bridge.broadcast_transaction(transaction)
        node.api.network_broadcast.broadcast_transaction(trx=transaction)


def __prepare_and_sign_transactions(wallet, all_operations: list[list[Operation]]) -> list[list[dict[str, Any]]]:
    def __sign_transaction_in_chunk(chunk):
        transactions_in_chunk = []
        for pack_operations in chunk:
            transactions_in_chunk.append(
                create_transaction_with_any_operation(wallet, *pack_operations, broadcast_transaction=False)
            )
        return transactions_in_chunk

    num_chunks = 20
    chunk_size = len(all_operations) // num_chunks
    chunks = [all_operations[i : i + chunk_size] for i in range(0, len(all_operations), chunk_size)]

    with ThreadPoolExecutor(max_workers=6) as executor:
        results = list(executor.map(__sign_transaction_in_chunk, chunks))

    return [item for sublist in results for item in sublist]


def __fast_broadcast(node: tt.InitNode, transactions: list) -> None:
    def a(chunk):
        for transaction in chunk:
            # TODO TUTAJ ZMIENIĆ API?
            # node.api.wallet_bridge.broadcast_transaction(transaction)
            node.api.network_broadcast.broadcast_transaction(trx=transaction)

    num_chunks = 1000
    chunk_size = len(transactions) // num_chunks
    chunks = [transactions[i : i + chunk_size] for i in range(0, len(transactions), chunk_size)]

    with ThreadPoolExecutor(max_workers=6) as executor:
        list(executor.map(a, chunks))


if __name__ == "__main__":
    prepare_second_stage_of_block_log("single_sign")
    # prepare_second_stage_of_block_log("multi_sign")
