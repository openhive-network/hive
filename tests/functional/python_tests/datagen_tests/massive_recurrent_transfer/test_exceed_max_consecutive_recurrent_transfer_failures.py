import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES


@run_for("testnet")
def test_exceed_max_consecutive_recurrent_transfer_failures(node):
    wallet = tt.Wallet(attach_to=node)

    too_many_recurrent_transfers_executions = MAX_CONSECUTIVE_RECURRENT_TRANSFER_FAILURES + 10

    wallet.create_account("sender", hives=tt.Asset.Test(10), vests=tt.Asset.Test(10))

    # Creation of recurrent transfer. The account of sender has the resources to send one transfer,
    # with the reset expected to fai
    wallet.api.recurrent_transfer("sender",
                                  "initminer",
                                  tt.Asset.Test(10),
                                  f"recurrent transfer to receiver",
                                  24,
                                  too_many_recurrent_transfers_executions)

    # Disabling the node to re-enable with a different date
    node.wait_for_irreversible_block()
    wallet.close()
    node.close()

    # Activating the same node with the date exceeding the date of the last recurrent transfer
    node.run(time_offset=tt.Time.serialize(tt.Time.from_now(days=11, serialize=False),
             format_=tt.Time.TIME_OFFSET_FORMAT) + " x20")

    # Validating that immediately after turning on the node no recurrent transfer was a fail
    assert len(node.api.account_history.enum_virtual_ops(block_range_begin=0,
                                                         block_range_end=100,
                                                         filter=17179869184)["ops"]) == 0

    # Waiting until ten recurrent transfers will process
    # Everyone block is trying to send next recurrent transfer unsuccessfully (sender account has no resources)
    node.wait_number_of_blocks(too_many_recurrent_transfers_executions)

    # Validating that after processing all recurrent transfers there is ten failed transfers
    # (10 virtual operations 'failed_recurrent_transfer_operation)
    assert len(node.api.account_history.enum_virtual_ops(block_range_begin=0,
                                                         block_range_end=100,
                                                         include_reversible=True,
                                                         filter=17179869184)["ops"]) == 10

    # Validating that recurrent transfer was deleted
    assert len(node.api.condenser.find_recurrent_transfers("sender")) == 0
