import math

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import MAX_RECURRENT_TRANSFER_END_DATE


@pytest.mark.parametrize("executions", [
    2,
    3,
    4,
    730  # The maximum number of executions where the minimal time between recurrences is not less than 24h
])
@run_for("testnet")
def test_exceed_date_limit_of_recurrent_transfers(node, executions):
    """
    By default, the maximum recurrence time to execute all sent recurrent transfers is 2 years (730 days).
    Because the first execution happens instantly on new recurrent transfer, the maximum is 730 days with two executions.
    The edge case for new recurrent transfer is recurrence = MAX_RECURRENT_TRANSFER_END_DATE * 24 / (executions - 1)
    which is still valid (exactly 730 days).
    It becomes more complicated with edits of existing recurrent transfer, because first execution after
    edit happens on previous schedule or after recurrence time when that parameter is changed, but the idea
    is the same - last transfer has to be scheduled for execution not further into the future than 2 years.

    If the maximum is exceeded, the exception 'Cannot set a transfer that would last for longer than 730 days' is thrown.
    """
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("receiver")
    wallet.create_account("sender",
                          hives=tt.Asset.Test(100),
                          vests=tt.Asset.Test(100))

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.recurrent_transfer("sender",
                                      "receiver",
                                      tt.Asset.Test(0.001),
                                      f"recurrent transfer to receiver",
                                      math.ceil(MAX_RECURRENT_TRANSFER_END_DATE * 24 / (executions - 1)) + 1, # 730 * 24 / 729 yields sub-hour results, so we increase it to the nearest higher
                                      executions)

    expected_error_message = f"Cannot set a transfer that would last for longer than " \
                             f"{MAX_RECURRENT_TRANSFER_END_DATE} days"
    assert expected_error_message in str(exception.value)


@pytest.mark.parametrize("executions", [2, 3, 4])
@run_for("testnet")
def test_last_execution_of_recurrent_transfer_close_to_date_limit(node, executions):
    """
    In this test, we validate that recurrent transfers are executed when the recurrence parameter is
    set to the maximum recurrent transfer time minus 1 hour.

    For 2 executions:
    ((730 days * 24 hours) / 2) - 1 = 8759 hours (364 days)
    """
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("receiver")
    wallet.create_account("sender",
                          hives=tt.Asset.Test(100),
                          vests=tt.Asset.Test(100))

    # Validate that sender's balance is equal 100 Tests
    assert node.api.condenser.get_accounts(['sender'])[0]["balance"] == '100.000 TESTS'
    # Validate that receiver's balance is equal 0 Tests
    assert node.api.condenser.get_accounts(['receiver'])[0]["balance"] == '0.000 TESTS'

    wallet.api.recurrent_transfer("sender",
                                  "receiver",
                                  tt.Asset.Test(10),
                                  f"recurrent transfer to receiver",
                                  (MAX_RECURRENT_TRANSFER_END_DATE * 24 / executions) - 1,
                                  executions)

    # Validate that sender's balance is equal 90 Tests
    assert node.api.condenser.get_accounts(['sender'])[0]["balance"] == '90.000 TESTS'
    # Validate that receiver's balance is equal 10 Tests
    assert node.api.condenser.get_accounts(['receiver'])[0]["balance"] == '10.000 TESTS'
