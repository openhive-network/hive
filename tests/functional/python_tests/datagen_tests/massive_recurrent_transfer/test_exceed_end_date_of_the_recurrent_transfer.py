import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import MAX_RECURRENT_TRANSFER_END_DATE


@run_for("testnet")
def test_exceed_end_date_of_the_recurrent_transfer(node):
    wallet = tt.Wallet(attach_to=node)
    executions = 2

    wallet.create_account("receiver")
    wallet.create_account("sender",
                          hives=tt.Asset.Test(100),
                          vests=tt.Asset.Test(100))

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.recurrent_transfer("sender",
                                      "receiver",
                                      tt.Asset.Test(0.001),
                                      f"recurrent transfer to receiver",
                                      # maximum time in hours to execute all order recurrent transfers
                                      MAX_RECURRENT_TRANSFER_END_DATE * 24 / executions,
                                      executions)

    expected_error_message = f"Cannot set a transfer that would last for longer than " \
                             f"{MAX_RECURRENT_TRANSFER_END_DATE} days"
    assert expected_error_message in str(exception.value)
