from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from datetime import timedelta
from functools import partial
from typing import Callable

import test_tools as tt
from schemas.fields.hive_int import HiveInt
from schemas.operation import Operation
from test_tools.__private.wallet.constants import SimpleTransaction, SimpleTransactionLegacy
from wax import get_tapos_data
from wax._private.result_tools import to_cpp_string


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
        for thread_number in tasks:
            thread_number.result()


def connect_nodes(first_node: tt.AnyNode, second_node: tt.AnyNode) -> None:
    """
    This place have to be removed after solving issue https://gitlab.syncad.com/hive/test-tools/-/issues/10
    """
    second_node.config.p2p_seed_node = first_node.p2p_endpoint.as_string()


def __generate_and_broadcast_transaction(
    wallet: tt.Wallet | tt.OldWallet, node: tt.InitNode, func: Callable, account_names: list[str], **kwargs
) -> None:
    gdpo = node.api.database.get_dynamic_global_properties()
    block_id = gdpo.head_block_id
    tapos_data = get_tapos_data(to_cpp_string(block_id))
    ref_block_num = tapos_data.ref_block_num
    ref_block_prefix = tapos_data.ref_block_prefix

    assert ref_block_num >= 0, f"ref_block_num value `{ref_block_num}` is invalid`"
    assert ref_block_prefix > 0, f"ref_block_prefix value `{ref_block_prefix}` is invalid`"

    transaction = (SimpleTransactionLegacy if isinstance(wallet, tt.OldWallet) else SimpleTransaction)(
        ref_block_num=HiveInt(ref_block_num),
        ref_block_prefix=HiveInt(ref_block_prefix),
        expiration=gdpo.time + timedelta(seconds=1800),
        extensions=[],
        signatures=[],
        operations=[],
    )

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
