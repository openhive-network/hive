"""
Oversized transactions in a run's loaded window: signed account metadata updates larger than a peer's send queue
holds, broadcast from a colony at their blocks and followed to the block each reaches.

Runs in the driver's container (test-tools signs them).
"""

from __future__ import annotations

import json
import threading
import time
from datetime import UTC, datetime
from typing import TYPE_CHECKING, Any

import test_tools as tt
from python.functional.p2p_load import json_rpc

if TYPE_CHECKING:
    from collections.abc import Callable

    from python.functional.p2p_load.spec import RunSpec

# the fixture's account with the resource credits for them: a 1.3 MB comment cost 2.3e11 of initminer's 2.5e15 RC,
# more than any other account it has keys for holds. A metadata update has no interval, unlike a post: hived reapplies
# a pending post after every block, which renews its author's last post time, so while one waits for a block the
# next post of its author is refused.
_ACCOUNT = "initminer"
_PADDING = "p2p load oversized transaction "
# hived admits a transaction up to maximum_block_size minus this
_BLOCK_OVERHEAD_BYTES = 256
# long enough to wait out any flood of the run
_EXPIRATION_S = 3600 - 1
_POLL_S = 1.0
_RPC_TIMEOUT_S = 60


class OversizedTransactions(threading.Thread):
    """
    Broadcast the spec's oversized transactions from `colony` at their loaded blocks and follow each to its block,
    until all are in one or the head reaches `end_block`.

    `chain_time` maps a real-clock moment to the nodes' faked clock (seconds since the epoch on both).
    """

    def __init__(
        self,
        spec: RunSpec,
        colony: tt.ApiNode | tt.WitnessNode,
        colony_start_block: int,
        end_block: int,
        chain_time: Callable[[float], float],
    ) -> None:
        super().__init__(daemon=True)
        self._spec = spec
        self._url = str(colony.http_endpoint)
        self._due = [(index, colony_start_block + offset) for index, offset in enumerate(spec.oversized_tx_blocks())]
        self._end_block = end_block
        self._chain_time = chain_time
        self.maximum_block_size = self._admitting_block_size()
        self._wallet = tt.Wallet(attach_to=colony)
        self._wallet.api.set_transaction_expiration(_EXPIRATION_S)
        self.transactions: list[dict[str, Any]] = []
        self._stop_event = threading.Event()
        self._error: Exception | None = None

    def _call(self, method: str, params: dict[str, Any]) -> Any:
        return json_rpc.call(self._url, method, params, _RPC_TIMEOUT_S)

    def _admitting_block_size(self) -> int:
        """Return the chain's maximum_block_size; raise RuntimeError unless it admits the transactions."""
        size = int(self._call("database_api.get_dynamic_global_properties", {})["maximum_block_size"])
        if size - _BLOCK_OVERHEAD_BYTES < self._spec.oversized_tx_bytes:
            raise RuntimeError(
                f"maximum_block_size {size} admits transactions of at most {size - _BLOCK_OVERHEAD_BYTES} bytes, "
                f"not the {self._spec.oversized_tx_bytes} of --oversized-txs"
            )
        return size

    def _head(self) -> int:
        return int(self._call("database_api.get_dynamic_global_properties", {})["head_block_number"])

    def _signed(self, index: int, padding_bytes: int) -> tuple[dict[str, Any], str, int]:
        """Return a signed metadata update with `padding_bytes` of padding, its id and its serialized size."""
        padding = (_PADDING * (padding_bytes // len(_PADDING) + 1))[:padding_bytes]
        metadata = json.dumps({"p2p_load_oversized_transaction": index, "padding": padding})
        signed = self._wallet.api.update_account_meta(_ACCOUNT, metadata, broadcast=False)
        transaction = json.loads(signed.json())
        size = len(self._call("database_api.get_transaction_hex", {"trx": transaction})["hex"]) // 2
        return transaction, str(signed.transaction_id), size

    def _broadcast(self, index: int, head: int) -> dict[str, Any]:
        """Sign oversized transaction `index` to exactly the spec's size, broadcast it from the colony, record it."""
        target = self._spec.oversized_tx_bytes
        _, _, size = self._signed(index, target)
        transaction, tx_id, size = self._signed(index, 2 * target - size)
        record: dict[str, Any] = {"index": index, "id": tx_id, "bytes": size, "account": _ACCOUNT}
        sent = time.time()
        try:
            self._call("network_broadcast_api.broadcast_transaction", {"trx": transaction, "max_block_age": -1})
            record["error"] = None
        except json_rpc.JsonRpcError as error:
            record["error"] = str(error)
        tt.logger.info(
            f"oversized transaction {index} ({size} B, {tx_id}) broadcast at block {head}: {record['error']}"
        )
        return {
            **record,
            "broadcast_block": head,
            "broadcast_t": sent,
            "broadcast_chain_time": self._chain_time(sent),
            "broadcast_s": time.time() - sent,
            "block": None,
        }

    def _follow(self) -> None:
        """Record the block of every broadcast transaction a block has taken since the last look."""
        for tx in self.transactions:
            if tx["error"] is not None or tx["block"] is not None:
                continue
            status = self._call("transaction_status_api.find_transaction", {"transaction_id": tx["id"]})
            if not str(status["status"]).startswith("within_") or "block_num" not in status:
                continue
            header = self._call("block_api.get_block_header", {"block_num": status["block_num"]})["header"]
            tx["block"] = int(status["block_num"])
            tx["block_time"] = datetime.fromisoformat(header["timestamp"]).replace(tzinfo=UTC).timestamp()
            tx["seen_t"] = time.time()
            tt.logger.info(f"oversized transaction {tx['index']} is in block {tx['block']}")

    def run(self) -> None:
        try:
            while not self._stop_event.is_set():
                head = self._head()
                while self._due and head >= self._due[0][1]:
                    self.transactions.append(self._broadcast(self._due.pop(0)[0], head))
                self._follow()
                settled = all(tx["block"] is not None or tx["error"] is not None for tx in self.transactions)
                if head >= self._end_block or (not self._due and settled):
                    return
                self._stop_event.wait(_POLL_S)
        except (OSError, RuntimeError, KeyError, ValueError) as error:  # finish() raises it in the driver's thread
            self._error = error

    def finish(self) -> list[dict[str, Any]]:
        """Stop following, and return the transactions; raise RuntimeError if the thread failed."""
        self._stop_event.set()
        self.join()
        self._wallet.close()
        if self._error is not None:
            raise RuntimeError(f"oversized transactions: {self._error!r}") from self._error
        return self.transactions
