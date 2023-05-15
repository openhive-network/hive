import wax
from consts import ENCODING

def test_serialize_transaction() -> None:
    trx_00 = b'{"ref_block_num":1111,"ref_block_prefix":61170152,"expiration":"2016-03-24T18:01:03","operations":[{"type":"pow_operation","value":{"worker_account":"steemit20","block_id":"00000457e861a5031091d4f023a0ea59940769da","nonce":158936,"work":{"worker":"STM65wH1LZ7BfSHcK69SShnqCAH5xdoSZpGkUjmzHJ5GCuxEK9V5G","input":"276060533b97694f0b7cec59d89e0457732e6356b56103eead518a4ec4b10abe","signature":"1f3328dd7fc9ffbcbbf838e4498b39ab22bc09327eccf4cf4e504b68b1eca2df336c8465ad38641639f114faeb4d357bea8dd3a9b898653c1eb259f75c5fc971fb","work":"0000117040ae389c5e52e5e8e0ac384f1c595ca98fcde7919b68ee7e8c73737a"},"props":{"account_creation_fee":{"amount":"100000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}}],"extensions":[],"signatures":[]}'
    result_00, buffer_00, exception_message_00 = wax.serialize_transaction(trx_00)
    assert result_00
    assert buffer_00.decode(ENCODING) == "5704e861a5035f2bf456010e09737465656d6974323000000457e861a5031091d4f023a0ea59940769dad86c020000000000029db013797711c88cccca3692407f9ff9b9ce7221aaa2d797f1692be2215d0a5f276060533b97694f0b7cec59d89e0457732e6356b56103eead518a4ec4b10abe1f3328dd7fc9ffbcbbf838e4498b39ab22bc09327eccf4cf4e504b68b1eca2df336c8465ad38641639f114faeb4d357bea8dd3a9b898653c1eb259f75c5fc971fb0000117040ae389c5e52e5e8e0ac384f1c595ca98fcde7919b68ee7e8c73737aa0860100000000002320bcbe00000200e80300"
    assert len(exception_message_00) == 0

    trx_01 = b'{}'
    result_01, buffer_01, exception_message_01 = wax.serialize_transaction(trx_01)
    assert result_01
    assert buffer_01.decode(ENCODING) == "000000000000000000000000"
    assert len(exception_message_01) == 0