import pytest

import wax

from consts import VALID_OPERATIONS, ENCODING


@pytest.mark.parametrize("operation_name", list(VALID_OPERATIONS.keys()))
def test_valid_operation(operation_name: str) -> None:
    assert wax.validate_operation(VALID_OPERATIONS[operation_name].encode(ENCODING))


def test_invalid_operation() -> None:
    invalid_op = """{"type":"transfer_operation","value":{"from":"initminer","to":"alpha","amount":"123.000 HIVE","memo":"test"}}"""
    assert wax.validate_operation(invalid_op.encode(ENCODING)) is False
