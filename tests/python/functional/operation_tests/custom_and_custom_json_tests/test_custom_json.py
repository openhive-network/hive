import pytest

import test_tools as tt

from hive_local_tools.functional.python.operation.custom_and_custom_json import CustomJson
from hive_local_tools.functional.python.operation import Account


@pytest.mark.testnet()
def test_custom_json(prepared_node: tt.InitNode, wallet: tt.Wallet, alice: Account, bob: Account, carol: Account) -> None:
    custom_json = CustomJson(prepared_node, wallet)
    json = "{\"key\": \"value\", ""key2\": \"value2\", ""key3\": \"value3\"}"
    operation = custom_json.generate_operation([], [alice.name], "test_id", json)
    print()
