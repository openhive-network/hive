from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from wax.exceptions import WaxValidationFailedError


@run_for("testnet")
def test_recurrence_time_is_less_than_24h(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("receiver")
    wallet.create_account("sender", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    with pytest.raises(WaxValidationFailedError) as exception:
        wallet.api.recurrent_transfer(
            "sender",
            "receiver",
            tt.Asset.Test(0.001),
            "recurrent transfer to receiver",
            recurrence=20,  # recurrence is less than 24h
            executions=2,
        )

    expected_error_message = "Cannot set a transfer recurrence that is less than"
    assert expected_error_message in exception.value.args[0]
