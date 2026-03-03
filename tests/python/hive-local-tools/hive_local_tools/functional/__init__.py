from __future__ import annotations

import contextlib
from concurrent.futures import ThreadPoolExecutor
from datetime import timedelta
from functools import partial
from typing import TYPE_CHECKING

import test_tools as tt
from schemas.fields.hive_int import HiveInt
from schemas.jsonrpc import get_response_model
from schemas.operation import Operation
from test_tools.__private.wallet.constants import SimpleTransaction, SimpleTransactionLegacy
from wax import get_tapos_data

if TYPE_CHECKING:
    from collections.abc import Callable


def _warmup_msgspec_decoders() -> None:
    """
    Pre-initialize msgspec decoders in the main thread to avoid race conditions.

    msgspec's decoder initialization involves Python's typing module internals
    (ForwardRef creation) which are not thread-safe. When multiple threads
    simultaneously create decoders for the first time, a segfault can occur.

    This function triggers decoder initialization for common response types
    before any threads are spawned, ensuring thread-safe operation.
    """
    with contextlib.suppress(Exception):
        # Import and access API types to trigger annotation resolution
        # This ensures msgspec decoders are initialized before parallel operations
        import hiveio_api.database_api  # noqa: F401

        import schemas.apis.app_status_api  # Used by beekeepy.__discover_ports()
        import schemas.apis.network_node_api
        import schemas.apis.wallet_bridge_api
        import schemas.transaction  # noqa: F401

        # Warm up the decoder cache by parsing sample responses
        get_response_model(str, '{"jsonrpc":"2.0","id":0,"result":"warmup"}', "hf26")
        get_response_model(dict, '{"jsonrpc":"2.0","id":0,"result":{}}', "hf26")
        get_response_model(list, '{"jsonrpc":"2.0","id":0,"result":[]}', "hf26")


def wait_for_current_hardfork(node: tt.InitNode, current_hardfork_number: int) -> None:
    def is_current_hardfork() -> bool:
        version = node.api.wallet_bridge.get_hardfork_version()
        return int(version.split(".")[1]) >= current_hardfork_number

    tt.logger.info("Wait for current hardfork...")
    tt.Time.wait_for(is_current_hardfork)
    tt.logger.info(f"Current Hardfork {current_hardfork_number} applied.")


def simultaneous_node_startup(
    nodes: list[tt.InitNode | tt.ApiNode],
    timeout: int,
    wait_for_live: bool,
    alternate_chain_specs: tt.AlternateChainSpecs | None = None,
    arguments: list | None = None,
    time_control: tt.StartTimeControl = None,
    exit_before_synchronization: bool = False,
) -> None:
    # Warm up msgspec decoders to avoid race conditions
    # Loguru threading issues are now handled by global configuration in conftest.py
    # (enqueue=False set at session scope before tests run)
    _warmup_msgspec_decoders()

    # Start nodes concurrently using ThreadPoolExecutor
    # This is safe now because loguru is configured for synchronous mode globally
    with ThreadPoolExecutor(max_workers=len(nodes)) as executor:
        tasks = []
        for node in nodes:
            tasks.append(
                executor.submit(
                    partial(
                        lambda _node: _node.run(
                            timeout=timeout,
                            alternate_chain_specs=alternate_chain_specs,
                            arguments=arguments or [],
                            wait_for_live=wait_for_live,
                            time_control=time_control,
                            exit_before_synchronization=exit_before_synchronization,
                        ),
                        node,
                    )
                )
            )
        for task in tasks:
            task.result()


def connect_nodes(first_node: tt.AnyNode, second_node: tt.AnyNode) -> None:
    """
    This place have to be removed after solving issue https://gitlab.syncad.com/hive/test-tools/-/issues/10
    """
    second_node.config.p2p_seed_node.append(first_node.p2p_endpoint)


def __generate_and_broadcast_transaction(
    wallet: tt.Wallet | tt.OldWallet, node: tt.InitNode, func: Callable, account_names: list[str], **kwargs
) -> None:
    gdpo = node.api.database.get_dynamic_global_properties()
    block_id = gdpo.head_block_id
    tapos_data = get_tapos_data(block_id)
    ref_block_num = tapos_data.ref_block_num
    ref_block_prefix = tapos_data.ref_block_prefix

    assert ref_block_num >= 0, f"ref_block_num value `{ref_block_num}` is invalid`"
    assert ref_block_prefix > 0, f"ref_block_prefix value `{ref_block_prefix}` is invalid`"

    default_transaction_data = {
        "ref_block_num": HiveInt(ref_block_num),
        "ref_block_prefix": HiveInt(ref_block_prefix),
        "expiration": gdpo.time + timedelta(seconds=1800),
        "extensions": [],
        "signatures": [],
        "operations": [],
    }

    if isinstance(wallet, tt.OldWallet):
        transaction = SimpleTransactionLegacy(
            **default_transaction_data,
            block_num=gdpo.head_block_number,
            transaction_id="0" * 40,
            transaction_num=0,
        )
    else:
        transaction = SimpleTransaction(**default_transaction_data)

    is_batch_mode = "executions" in kwargs and "recurrence" in kwargs

    for num, name in enumerate(account_names):
        if is_batch_mode:
            receivers = account_names.copy()
            receivers.remove(name)
            for receiver in receivers:
                transaction.add_operation(func(name, receiver, **kwargs))
            sign_transaction = wallet.api.sign_transaction(transaction, broadcast=False)
            node.api.network_broadcast.broadcast_transaction(trx=sign_transaction)
            tt.logger.info(f"Finished account: {name}, {num}/{len(account_names)}")
            transaction.operations.clear()
        else:
            ret = func(name, **kwargs)
            if isinstance(ret, Operation):
                transaction.add_operation(ret)
            else:
                [transaction.add_operation(op) for op in ret]

    if not is_batch_mode and transaction.operations:
        sign_transaction = wallet.api.sign_transaction(transaction, broadcast=False)
        node.api.network_broadcast.broadcast_transaction(trx=sign_transaction)

    tt.logger.info(f"Finished: {account_names[0]} to {account_names[-1]}")
