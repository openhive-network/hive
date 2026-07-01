from __future__ import annotations

import pytest

import test_tools as tt
from hive_local_tools import run_for
from test_tools.exceptions import ErrorInResponseError


@run_for("testnet")
def test_one_execution_of_the_recurrent_transfer(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)

    wallet.create_account("receiver")
    wallet.create_account("sender", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    # Since HF29 the "at least 2 executions when creating" rule is enforced by the evaluator instead of during
    # operation validation, so creating a recurrent transfer with a single execution is now rejected by the node
    # at broadcast time (ErrorInResponseError) rather than by wax during validation (WaxAssertionError).
    with pytest.raises(ErrorInResponseError) as exception:
        wallet.api.recurrent_transfer(
            "sender",
            "receiver",
            tt.Asset.Test(0.001),
            "recurrent transfer to receiver",
            recurrence=24,
            executions=1,
        )

    expected_error_message = (
        "Executions must be at least 2, if you set executions to 1 the recurrent transfer will execute immediately and"
        " delete itself. You should use a normal transfer operation"
    )

    assert expected_error_message in str(exception.value)
