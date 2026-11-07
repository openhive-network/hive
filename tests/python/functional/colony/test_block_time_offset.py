from __future__ import annotations

import json
import statistics
from typing import TYPE_CHECKING, Any

import pytest

import test_tools as tt
from hive_local_tools.functional import simultaneous_node_startup
from test_tools import complex_networks as ttcn

from .test_colony import WITNESSES, prepare_colony_config, wait_for_start_colony

if TYPE_CHECKING:
    from pathlib import Path

TRANSACTIONS_PER_BLOCK = 2500
MIN_TRX_IN_BLOCK = TRANSACTIONS_PER_BLOCK * 90 // 100
COLLECTED_BLOCKS = 60
DRIFT_WINDOW = 20
MAX_P95_OFFSET_MS = 600
MAX_DRIFT_MS = 150

_BLOCK_STATS_MARKER = "Block stats:"
_UNAPPLIED_BLOCK_TYPES = {"broken", "forked", "ignored"}


@pytest.mark.testnet
def test_block_time_offset_under_sustained_load(
    block_log_single_sign: tt.BlockLog, alternate_chain_spec: tt.AlternateChainSpecs
) -> None:
    """
    ColonyNode ●───────────● WitnessNode ●───────────● ObserverNode

    Under sustained load of 2500 transactions per block, the observer must receive each block within a bounded
    offset from its timestamp, and that offset must not grow over time.
    """
    witness_node = tt.WitnessNode(witnesses=["initminer", *WITNESSES])
    witness_node.config.shared_file_size = "4G"
    for witness in WITNESSES:
        witness_node.config.private_key.append(tt.Account(witness).private_key)
    witness_node.config.private_key.append(tt.Account("initminer").private_key)

    colony_node = tt.ApiNode()
    colony_node.config.shared_file_size = "4G"
    prepare_colony_config(colony_node)
    colony_node.config.colony_transactions_per_block = str(TRANSACTIONS_PER_BLOCK)
    colony_node.config.colony_start_at_block = block_log_single_sign.get_head_block_number()

    observer_node = tt.ApiNode()
    observer_node.config.shared_file_size = "4G"

    colony_node.config.p2p_endpoint, witness_node.config.p2p_endpoint = ttcn.generate_free_addresses(2)
    witness_node.config.p2p_seed_node = colony_node.config.p2p_endpoint
    observer_node.config.p2p_seed_node = witness_node.config.p2p_endpoint

    nodes = [witness_node, colony_node, observer_node]
    for node in nodes:
        node.run(
            replay_from=block_log_single_sign,
            timeout=180,
            alternate_chain_specs=alternate_chain_spec,
            exit_before_synchronization=True,
        )

    simultaneous_node_startup(
        nodes,
        timeout=180,
        alternate_chain_specs=alternate_chain_spec,
        wait_for_live=True,
        time_control=tt.StartTimeControl(start_time=block_log_single_sign.get_head_block_time()),
    )

    wait_for_start_colony(observer_node, min_trx_in_block=MIN_TRX_IN_BLOCK, timeout=420)

    saturated_at = observer_node.get_last_block_number()
    collected = range(saturated_at + 1, saturated_at + 1 + COLLECTED_BLOCKS)
    # The stats line is logged after the block is applied, so wait one block more to see the last one's.
    observer_node.wait_for_block_with_number(collected[-1] + 1, timeout=COLLECTED_BLOCKS * 3 * 2)

    stats = _read_block_stats(observer_node.directory / "stderr.log")
    missing = [num for num in collected if num not in stats]
    assert not missing, f"Observer logged no Block stats for blocks {missing}"

    underfilled = {num: stats[num]["txs"] for num in collected if stats[num]["txs"] < MIN_TRX_IN_BLOCK}
    assert not underfilled, f"Blocks with fewer than {MIN_TRX_IN_BLOCK} transactions after saturation: {underfilled}"

    offsets_ms = [stats[num]["offset"] / 1000 for num in collected]
    tt.logger.info(f"Observer block time offsets (ms) for blocks {collected[0]}-{collected[-1]}: {offsets_ms}")

    p95_offset_ms = statistics.quantiles(offsets_ms, n=20, method="inclusive")[-1]
    assert p95_offset_ms <= MAX_P95_OFFSET_MS, (
        f"Observer p95 block time offset {p95_offset_ms:.1f} ms exceeds {MAX_P95_OFFSET_MS} ms"
    )

    drift_ms = statistics.median(offsets_ms[-DRIFT_WINDOW:]) - statistics.median(offsets_ms[:DRIFT_WINDOW])
    assert drift_ms <= MAX_DRIFT_MS, (
        f"Observer block time offset drifted by {drift_ms:.1f} ms over {COLLECTED_BLOCKS} blocks"
        f" (limit {MAX_DRIFT_MS} ms)"
    )


def _read_block_stats(log_path: Path) -> dict[int, dict[str, Any]]:
    """Return the last applied-block report per block number from the node's `Block stats:{...}` log lines."""
    decoder = json.JSONDecoder()
    stats: dict[int, dict[str, Any]] = {}
    for line in log_path.read_text(errors="replace").splitlines():
        _, marker, payload = line.partition(_BLOCK_STATS_MARKER)
        if not marker:
            continue
        report, _ = decoder.raw_decode(payload)
        if report["type"] not in _UNAPPLIED_BLOCK_TYPES:
            stats[report["num"]] = report
    return stats
