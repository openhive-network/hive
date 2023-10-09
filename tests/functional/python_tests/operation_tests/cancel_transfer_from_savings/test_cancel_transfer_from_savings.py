import pytest
import test_tools as tt
from hive_local_tools.functional.python.operation import check_if_fill_transfer_from_savings_vop_was_generated


@pytest.mark.parametrize(
    "currency, check_savings_balance, check_balance",
    (
        # transfers from savings in HIVES
        (tt.Asset.Test, "get_hive_savings_balance", "get_hive_balance"),
        # transfers from savings in HBDS
        (tt.Asset.Tbd, "get_hbd_savings_balance", "get_hbd_balance"),
    ),
)
@pytest.mark.testnet
def test_cancel_transfer_from_savings_simplest_scenario(
    prepared_node, wallet, alice, currency, check_savings_balance, check_balance
):
    alice.transfer_to_savings("alice", currency(50), "transfer to savings")
    funds_after_transfer_to_savings = getattr(alice, check_balance)()
    balance_before_withdrawal = getattr(alice, check_savings_balance)()
    rc_amount_before_sending_op = alice.get_rc_current_mana()

    alice.transfer_from_savings(0, "alice", currency(50), "transfer from savings")

    rc_amount_after_sending_op = alice.get_rc_current_mana()
    balance_after_withdrawal = getattr(alice, check_savings_balance)()

    assert balance_before_withdrawal == balance_after_withdrawal + currency(
        50
    ), f"{currency.token} savings balance wasn't decreased after withdrawal"
    assert rc_amount_before_sending_op > rc_amount_after_sending_op, "RC amount after hive withdrawal wasn't decreased."
    prepared_node.wait_for_irreversible_block()

    prepared_node.restart(
        time_offset=tt.Time.serialize(
            prepared_node.get_head_block_time() + tt.Time.days(2), format_=tt.Time.TIME_OFFSET_FORMAT
        )
    )

    savings_balance_before_withdrawal_cancellation = getattr(alice, check_savings_balance)()
    wallet.api.cancel_transfer_from_savings("alice", 0)
    savings_balance_after_withdrawal_cancellation = getattr(alice, check_savings_balance)()

    # TODO: ADD RC CHECK WHEN ISSUE WILL BE RESOLVED: https://gitlab.syncad.com/hive/hive/-/issues/507
    assert (
        savings_balance_before_withdrawal_cancellation + currency(50) == savings_balance_after_withdrawal_cancellation
    ), f"{currency.token}S from cancelled withdrawal didn't come back to savings balance"

    prepared_node.restart(
        time_offset=tt.Time.serialize(
            prepared_node.get_head_block_time() + tt.Time.days(1), format_=tt.Time.TIME_OFFSET_FORMAT
        )
    )

    assert getattr(alice, check_balance)() - funds_after_transfer_to_savings == currency(
        0
    ), f"{currency.token}S from cancelled withdrawal arrived to receiver account {currency.token} balance"


@pytest.mark.parametrize(
    "currency, check_savings_balance, check_balance",
    (
        # transfers from savings in HIVES
        (tt.Asset.Test, "get_hive_savings_balance", "get_hive_balance"),
        # transfers from savings in HBDS
        (tt.Asset.Tbd, "get_hbd_savings_balance", "get_hbd_balance"),
    ),
)
@pytest.mark.testnet
def test_cancel_all_transfers_from_savings(
    prepared_node, wallet, alice, currency, check_savings_balance, check_balance
):
    create_three_savings_withdrawals_from_fresh_account(
        prepared_node, currency, alice, check_savings_balance, currency.token
    )
    funds_after_transfer_to_savings = getattr(alice, check_balance)()
    for withdrawal_to_cancel in range(3):
        # TODO: ADD RC CHECK WHEN ISSUE WILL BE RESOLVED: https://gitlab.syncad.com/hive/hive/-/issues/507
        wallet.api.cancel_transfer_from_savings("alice", withdrawal_to_cancel)

    prepared_node.restart(
        time_offset=tt.Time.serialize(
            prepared_node.get_head_block_time() + tt.Time.days(3), format_=tt.Time.TIME_OFFSET_FORMAT
        )
    )

    assert getattr(alice, check_balance)() - funds_after_transfer_to_savings == currency(
        0
    ), f"At least one of cancelled withdrawals arrived to receiver account {currency.token} balance"


@pytest.mark.parametrize(
    "currency, check_savings_balance, check_balance",
    (
        # transfers from savings in HIVES
        (tt.Asset.Test, "get_hive_savings_balance", "get_hive_balance"),
        # transfers from savings in HBDS
        (tt.Asset.Tbd, "get_hbd_savings_balance", "get_hbd_balance"),
    ),
)
@pytest.mark.testnet
def test_cancel_all_transfers_from_savings_except_one(
    prepared_node, wallet, alice, currency, check_savings_balance, check_balance
):
    create_three_savings_withdrawals_from_fresh_account(
        prepared_node, currency, alice, check_savings_balance, currency.token
    )
    funds_after_transfer_to_savings = getattr(alice, check_balance)()

    for withdrawal_to_cancel in (0, 2):
        # TODO: ADD RC CHECK WHEN ISSUE WILL BE RESOLVED: https://gitlab.syncad.com/hive/hive/-/issues/507
        wallet.api.cancel_transfer_from_savings("alice", withdrawal_to_cancel)

    prepared_node.restart(
        time_offset=tt.Time.serialize(
            prepared_node.get_head_block_time() + tt.Time.days(3), format_=tt.Time.TIME_OFFSET_FORMAT
        )
    )

    assert getattr(alice, check_balance)() - funds_after_transfer_to_savings == currency(10), (
        f"At least one of cancelled withdrawals arrived to receiver account {currency.token} balance or the "
        "withdrawal that wasn't cancelled didn't arrive."
    )

    assert (
        check_if_fill_transfer_from_savings_vop_was_generated(prepared_node, "transfer from savings with id: 1") is True
    ), "fill_transfer_from_savings from transfer with id 1 wasn't generated"


def create_three_savings_withdrawals_from_fresh_account(node, currency, account, check_savings_balance, token):
    account.transfer_to_savings("alice", currency(30), "transfer to savings")

    amount = currency(5)
    withdrawal_id = 0
    for time_offset in (6, 12, 18):
        balance_before_withdrawal = getattr(account, check_savings_balance)()
        rc_amount_before_sending_op = account.get_rc_current_mana()
        account.transfer_from_savings(withdrawal_id, "alice", amount, f"transfer from savings with id: {withdrawal_id}")
        rc_amount_after_sending_op = account.get_rc_current_mana()
        balance_after_withdrawal = getattr(account, check_savings_balance)()
        assert (
            rc_amount_before_sending_op > rc_amount_after_sending_op
        ), f"RC amount after {token} withdrawal wasn't decreased."
        assert (
            balance_before_withdrawal == balance_after_withdrawal + amount
        ), f"{token} savings balance wasn't decreased after withdrawal"

        amount += currency(5)
        withdrawal_id += 1
        node.restart(
            time_offset=tt.Time.serialize(
                node.get_head_block_time() + tt.Time.hours(time_offset), format_=tt.Time.TIME_OFFSET_FORMAT
            )
        )
