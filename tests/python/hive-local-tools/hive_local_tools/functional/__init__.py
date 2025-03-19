from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from functools import partial

import test_tools as tt


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
