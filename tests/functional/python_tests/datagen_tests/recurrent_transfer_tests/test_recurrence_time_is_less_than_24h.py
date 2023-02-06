import pytest

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_recurrence_time_is_less_than_24h(node):
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
                                      recurrence=20,  # recurrence is less than 24h
                                      executions=2)

    expected_error_message = f"Cannot set a transfer recurrence that is less than 24 hours"
    assert expected_error_message in str(exception.value)
