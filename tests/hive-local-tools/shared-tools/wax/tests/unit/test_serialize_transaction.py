import pytest
import wax

from consts import ENCODING, VALID_TRXS

@pytest.mark.parametrize("key", list(VALID_TRXS.keys()))
def test_serialize_transaction(key : str) -> None:
    result, buffer, exception_message = wax.serialize_transaction(VALID_TRXS[key]["trx"].encode(ENCODING))
    assert result
    assert buffer.decode(ENCODING) == VALID_TRXS[key]["hash"]
    assert len(exception_message) == 0
