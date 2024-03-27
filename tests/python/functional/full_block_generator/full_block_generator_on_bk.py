from __future__ import annotations

import asyncio
import os
from pathlib import Path
from typing import Final, Literal
import copy
import json
import requests
from shutil import rmtree

import test_tools as tt

from beekeepy import beekeeper_factory
from beekeepy.settings import Settings
from clive_local_tools.data.models import Keys, WalletInfo
from tests.functional.full_block_generator.generate_block_log_with_varied_signature_types import CHAIN_ID, WITNESSES
from generate_operations import generate_blocks

SIGNATURE_TYPE: Literal["open_sign", "single_sign", "multi_sign"] = "single_sign"
BLOCK_LOG_DIRECTORY: Final[Path] = Path(
    f"/home/dev/clive/tests/functional/full_block_generator/block_log_{SIGNATURE_TYPE}")

# Node parameters
SHARED_MEMORY_FILE_DIRECTORY: Final[str] = "/home/dev/Documents/full_block_generator/"
SHARED_MEMORY_FILE_SIZE: Final[int] = 24
WEBSERVER_THREAD_POOL_SIZE: Final[int] = 16

# Processes parameters
SIGNING_MAX_WORKERS: Final[int] = 63
BROADCASTING_MAX_WORKERS: Final[int] = 16

# Block parameters
STOP_AT_BLOCK: int | None = None
OPERATIONS_IN_TRANSACTION: Final[int] = 1
# 70 (4200 trx for open_sign), 60 ( 3780 trx for single_sign ), 40 (2400 trx for multi_sign)
TRANSACTIONS_IN_ONE_BLOCK: Final[int] = SIGNING_MAX_WORKERS * 60


async def full_block_generator(signature_type: Literal["open_sign", "multi_sign", "single_sign"]) -> None:
    # fixme: delete after repair ( https://gitlab.syncad.com/hive/test-tools/-/issues/44 ). From this
    # clive_path = Path("/home/dev/.clive")
    # if not os.path.exists(clive_path):
    #     os.makedirs(clive_path)
    # if os.path.exists(clive_path / "beekeeper"):
    #     rmtree(clive_path / "beekeeper", ignore_errors=False, onerror=None)
    # generated_directory = Path(__file__).parent / "generated"
    # if not os.path.exists(generated_directory):
    #     os.makedirs(generated_directory)
    # fixme: To this

    block_log = tt.BlockLog(BLOCK_LOG_DIRECTORY / "block_log")
    alternate_chain_spec_path = BLOCK_LOG_DIRECTORY / tt.AlternateChainSpecs.FILENAME

    node = tt.InitNode()

    node.config.plugin.remove("account_by_key")
    node.config.plugin.remove("state_snapshot")
    node.config.plugin.remove("account_by_key_api")
    node.config.shared_file_size = f"{SHARED_MEMORY_FILE_SIZE}G"
    node.config.webserver_thread_pool_size = f"{str(WEBSERVER_THREAD_POOL_SIZE)}"
    node.config.log_logger = (
        '{"name":"default","level":"info","appender":"stderr"} '
        '{"name":"user","level":"debug","appender":"stderr"} '
        '{"name":"chainlock","level":"error","appender":"p2p"} '
        '{"name":"sync","level":"debug","appender":"p2p"} '
        '{"name":"p2p","level":"debug","appender":"p2p"}'
    )

    for witness in WITNESSES:
        key = tt.Account(witness).private_key
        node.config.witness.append(witness)
        node.config.private_key.append(key)

    node.run(
        replay_from=block_log,
        time_control=tt.Time.serialize(block_log.get_head_block_time(), format_=tt.TimeFormats.FAKETIME_FORMAT),
        timeout=120,
        wait_for_live=True,
        alternate_chain_specs=tt.AlternateChainSpecs.parse_file(alternate_chain_spec_path),
        arguments=[f"--shared-file-dir={SHARED_MEMORY_FILE_DIRECTORY}", f"--chain-id={CHAIN_ID}"],
    )

    wallet = WalletInfo(name="my_only_wallet", password="my_password", keys=Keys(count=0))
    beekeeper = beekeeper_factory(settings=Settings(working_directory=tt.context.get_current_directory() / "beekeeper"))

    with beekeeper.create_session() as session:
        with session.create_wallet(name=wallet.name, password=wallet.password) as w:
            import_keys_to_beekeeper(w, signature_type)
            wallets = []
            for _ in range(SIGNING_MAX_WORKERS):
                session = beekeeper.create_session()
                wallets.append(session.open_wallet(name=wallet.name, password=wallet.password))

        # Create signing transactions
        generate_blocks(
            stop_at_block=STOP_AT_BLOCK,
            ops_in_one_element=OPERATIONS_IN_TRANSACTION,
            elements_number_for_iteration=TRANSACTIONS_IN_ONE_BLOCK,
            tokens=tokens,
            beekeeper_url=getattr(beekeeper, "_Beekeeper__instance").http_endpoint.as_string,
            node=node,
            max_broadcast_workers=BROADCASTING_MAX_WORKERS,
            public_keys=get_public_keys(signature_type)["active"],
            wallets=wallets,
            use_request=False,
        )



def import_keys_to_beekeeper(wallet, signature_type: str):
    match signature_type:
        case "single_sign":
            for authority_type in ["owner", "active", "posting"]:
                wallet.import_key(private_key=tt.Account("account", secret=authority_type).private_key)
        case "multi_sign":
            for authority_type, num_keys in [("owner", 3), ("active", 6), ("posting", 10)]:
                for num in range(num_keys):
                    wallet.import_key(private_key=tt.Account("account", secret=f"{authority_type}-{num}").private_key)


def start_session(url, chunk_num):
    headers = {'Content-Type': 'application/json'}
    template = {
        "jsonrpc": "2.0",
        "method": "",
        "params": {},
        "id": 1
    }

    create_session = copy.deepcopy(template)
    create_session["method"] = "beekeeper_api.create_session"
    create_session["params"] = {
        "salt": chunk_num,
        "notifications_endpoint": url
    }
    response_token = requests.post(url, json=create_session, headers=headers)
    token = json.loads(response_token.text)["result"]["token"]

    unlock = copy.deepcopy(template)
    unlock["method"] = "beekeeper_api.unlock"
    unlock["params"] = {
        "token": token,
        "wallet_name": "my_only_wallet",
        "password": "my_password",
    }
    requests.post(url, json=unlock, headers=headers)

    return token


def get_public_keys(authority_type: Literal["open_sign", "multi_sign", "single_sign"]) -> dict[str:list]:
    match authority_type:
        case "open_sign":
            return {
                "owner": [],
                "active": [],
                "posting": [],
            }
        case "single_sign":
            return {
                "owner": [tt.Account("account", secret="owner").public_key[3:]],
                "active": [tt.Account("account", secret="active").public_key[3:]],
                "posting": [tt.Account("account", secret="posting").public_key[3:]],
            }
        case "multi_sign":
            return {
                "owner": [tt.Account("account", secret=f"owner-{num}").public_key[3:] for num in range(3)],
                "active": [tt.Account("account", secret=f"active-{num}").public_key[3:] for num in range(6)],
                "posting": [tt.Account("account", secret=f"posting-{num}").public_key[3:] for num in range(10)],
            }


if __name__ == "__main__":
    asyncio.run(full_block_generator("single_sign"))
