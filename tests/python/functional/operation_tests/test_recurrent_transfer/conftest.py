from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools.functional.python.operation import jump_to_date
from hive_local_tools.functional.python.operation.recurrent_transfer import RecurrentTransfer, RecurrentTransferAccount

from .test_recurrent_transfer_with_extension import RECURRENT_TRANSFER_DEFINITIONS


@pytest.fixture()
def node(speed_up_node) -> tt.InitNode:
    speed_up_node.set_vest_price(quote=tt.Asset.Vest(1800))
    speed_up_node.wait_for_block_with_number(30)
    return speed_up_node


@pytest.fixture()
def wallet(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture()
def receiver(node, wallet):
    wallet.create_account("receiver")
    receiver = RecurrentTransferAccount("receiver", node, wallet)
    receiver.update_account_info()
    return receiver


@pytest.fixture()
def sender(request, node, wallet):
    params = request.node.callspec.params
    amount = [params[amount] for amount in params.keys() if "amount" in amount]
    execution = [params[execution] for execution in params.keys() if "execution" in execution]
    if len(amount) != len(execution):
        amount = [amount[0] for _ in range(len(execution))]
    top_up_sum = sum([amount * execution for amount, execution in zip(amount, execution)])

    wallet.create_account(
        name="sender",
        hives=top_up_sum if isinstance(top_up_sum, tt.Asset.Test) else None,
        hbds=top_up_sum if isinstance(top_up_sum, tt.Asset.Tbd) else None,
        vests=tt.Asset.Test(1),
    )
    sender = RecurrentTransferAccount("sender", node, wallet)
    sender.update_account_info()
    return sender


@pytest.fixture()
def recurrent_transfer_setup(request, node, wallet, sender, receiver):
    """
    Part of the common for recurrent_transfer_with_extensions test scenarios.
    Timeline:
        0h                             24h                            48h
        │‾‾‾‾‾‾‾recurrence time‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾recurrence time‾‾‾‾‾‾‾‾│
    ────■──────────────────────────────■──────────────●───────────────■─────────────────────────────────────────────>[t]
        ↓                              ↓              ↓
      Create                         Create RTD2    The starting point
      RTD1                           and RTD3       for extended tests
    """
    tt.logger.info("Start - common part of all tests")

    asset = request.node.callspec.params["asset"]

    sender.top_up(asset(sum(rtd.amount * rtd.executions for rtd in RECURRENT_TRANSFER_DEFINITIONS)))

    recurrent_transfers = []
    for iteration in range(3):
        recurrent_transfers.append(
            RecurrentTransfer(
                node,
                wallet,
                from_=sender.name,
                to=receiver.name,
                amount=asset(RECURRENT_TRANSFER_DEFINITIONS[iteration].amount),
                recurrence=RECURRENT_TRANSFER_DEFINITIONS[iteration].recurrence,
                executions=RECURRENT_TRANSFER_DEFINITIONS[iteration].executions,
                pair_id=iteration,
            )
        )
        sender.assert_balance_is_reduced_by_transfer(recurrent_transfers[iteration].amount)
        receiver.assert_balance_is_increased_by_transfer(recurrent_transfers[iteration].amount)
        sender.rc_manabar.assert_rc_current_mana_is_reduced(
            operation_rc_cost=recurrent_transfers[iteration].rc_cost,
            operation_timestamp=recurrent_transfers[iteration].timestamp,
        )
        recurrent_transfers[iteration].assert_fill_recurrent_transfer_operation_was_generated(
            expected_vop=iteration + 1
        )
        sender.update_account_info()
        receiver.update_account_info()
        if iteration == 0:
            jump_to_date(
                node,
                recurrent_transfers[iteration].timestamp
                + tt.Time.hours(RECURRENT_TRANSFER_DEFINITIONS[iteration].recurrence / 2),
            )

    node.wait_number_of_blocks(3)  # Extending the time between subsequent transfers
    tt.logger.info("Finish - common part of tests.")

    return node, wallet, sender, receiver, *recurrent_transfers
