import pytest
import wax
from consts import MAINNET_CHAIN_ID, VALID_IDS_WITH_TRANSACTIONS, ENCODING


@pytest.mark.parametrize("trx_id", list(VALID_IDS_WITH_TRANSACTIONS.keys()))
def test_proper_digest(trx_id: str) -> None:
    result, digest, exception_message = wax.calculate_digest(
        VALID_IDS_WITH_TRANSACTIONS[trx_id].encode(ENCODING), MAINNET_CHAIN_ID
    )
    assert result is True
    assert digest.decode(ENCODING) == trx_id
    assert len(exception_message) == 0