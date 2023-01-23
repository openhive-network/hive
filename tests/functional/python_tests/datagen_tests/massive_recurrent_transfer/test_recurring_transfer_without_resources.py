import pytest

import test_tools as tt
from hive_local_tools import run_for


@run_for("testnet")
def test_recurrent_transfer_without_resources(node):
    # "sender" want to send recurrent transfer, but it does not have resources
    wallet = tt.Wallet(attach_to=node)

    executions = 2

    wallet.create_account("sender")

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.recurrent_transfer("sender",
                                      "initminer",
                                      tt.Asset.Test(100),
                                      f"recurrent transfer to receiver",
                                      24,
                                      executions)

    expected_error_message = f"Account does not have enough tokens for the first transfer, has"
    assert expected_error_message in str(exception.value)
