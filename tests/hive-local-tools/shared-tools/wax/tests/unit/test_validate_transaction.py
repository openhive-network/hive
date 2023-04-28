import pytest
import wax
from consts import VALID_IDS_WITH_TRANSACTIONS, ENCODING


@pytest.mark.parametrize("trx_id", list(VALID_IDS_WITH_TRANSACTIONS.keys()))
def test_valid_transaction(trx_id: str) -> None:
    result = wax.validate_transaction(VALID_IDS_WITH_TRANSACTIONS[trx_id].encode(ENCODING))
    assert result is True
