import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES


@run_for("testnet")
def test_exceed_max_consecutive_recurrent_transfer_failures(node):
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("sender", hives=tt.Asset.Test(10), vests=tt.Asset.Test(10))

    # Creation of recurrent transfer. The account of sender has the resources to send only one transfer,
    # with the rest expected to fail
    wallet.api.recurrent_transfer("sender",
                                  "initminer",
                                  tt.Asset.Test(10),
                                  f"recurrent transfer to receiver",
                                  24,
                                  MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES + 1)

    node.wait_for_irreversible_block()
    wallet.close()
    node.close()

    # Run a node with a date that exceeds the last of the recurrent transfers.
    # It causes the cumulative execution of all unaccomplished recurrent transfers.
    node.run(time_offset=tt.Time.serialize(
        tt.Time.from_now(days=MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES + 1, serialize=False),
        format_=tt.Time.TIME_OFFSET_FORMAT))

    # Validating that immediately after turning on the node no recurrent transfer was a fail
    assert len(node.api.account_history.enum_virtual_ops(block_range_begin=0,
                                                         block_range_end=100,
                                                         filter=17179869184)["ops"]) == 0

    # Waiting until ten recurrent transfers will process
    # Every block is trying to send next recurrent transfer unsuccessfully (sender account has no resources)
    node.wait_number_of_blocks(MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES + 1)

    # Validating if a virtual operation `failed_recurrent_transfer_operation` has been processed 10 times.
    assert len(node.api.account_history.enum_virtual_ops(
        block_range_begin=0,
        block_range_end=node.get_last_block_number(),
        include_reversible=True,
        filter=17179869184)[
                   "ops"]) == MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES

    # Validating that recurrent transfer was deleted
    assert len(node.api.condenser.find_recurrent_transfers("sender")) == 0
