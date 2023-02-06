import pytest

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_one_execution_of_the_recurrent_transfer(node):
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
                                      recurrence=24,
                                      executions=1)

    expected_error_message = f"Executions must be at least 2, if you set executions to 1 " \
                             f"the recurrent transfer will execute immediately and delete itself."

    assert expected_error_message in str(exception.value)
