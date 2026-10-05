from __future__ import annotations

from typing import TYPE_CHECKING

from hive_local_tools import run_for

if TYPE_CHECKING:
    import test_tools as tt


@run_for("mainnet_5m")
def test_if_witness_creation_dates_are_not_set_to_epoch(
    node: tt.InitNode | tt.RemoteNode,
) -> None:
    # Witnesses are never removed, so their ids are 0 .. witness_count - 1. Ask only for existing ids:
    # get_witnesses returns null for unknown ones, which its typed response does not accept.
    witness_count = node.api.condenser.get_witness_count()
    batch_size = 1000

    # Zeroth witness it is an 'initminer' (id: 0) artificially created with date "1970-01-01T00:00:00". Loop starts from first real witness.
    for first_witness_id in range(1, witness_count, batch_size):
        witness_ids = list(range(first_witness_id, min(first_witness_id + batch_size, witness_count)))
        for witness in node.api.condenser.get_witnesses(witness_ids):
            assert witness.created != "1970-01-01T00:00:00", f"witness {witness.owner} has epoch creation date"
