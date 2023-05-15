import pytest

import wax

from consts import VALID_OPERATIONS, ENCODING


@pytest.mark.parametrize("operation_name", list(VALID_OPERATIONS.keys()))
def test_valid_operation(operation_name: str) -> None:
    result, exception_message = wax.validate_operation(VALID_OPERATIONS[operation_name].encode(ENCODING))
    assert result
    assert len(exception_message) == 0


def test_invalid_operation() -> None:
    invalid_op = """{"type":"transfer_operation","value":{"from":"initminer","to":"alpha","amount":"123.000 HIVE","memo":"test"}}"""
    result, exception_message = wax.validate_operation(invalid_op.encode(ENCODING))
    assert result is False
    assert len(exception_message) != 0