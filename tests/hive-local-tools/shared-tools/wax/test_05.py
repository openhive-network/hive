import example

trx_00 = b'{"ref_block_num":1111,"ref_block_prefix":61170152,"expiration":"2016-03-24T18:01:03","operations":[{"type":"pow_operation","value":{"worker_account":"steemit20","block_id":"00000457e861a5031091d4f023a0ea59940769da","nonce":158936,"work":{"worker":"STM65wH1LZ7BfSHcK69SShnqCAH5xdoSZpGkUjmzHJ5GCuxEK9V5G","input":"276060533b97694f0b7cec59d89e0457732e6356b56103eead518a4ec4b10abe","signature":"1f3328dd7fc9ffbcbbf838e4498b39ab22bc09327eccf4cf4e504b68b1eca2df336c8465ad38641639f114faeb4d357bea8dd3a9b898653c1eb259f75c5fc971fb","work":"0000117040ae389c5e52e5e8e0ac384f1c595ca98fcde7919b68ee7e8c73737a"},"props":{"account_creation_fee":{"amount":"100000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}}],"extensions":[],"signatures":[]}'
result_00 = example.validate_transaction(trx_00)
print( f"{trx_00} = result: {result_00}" )
print("====================================================================")

trx_01 = b'{"ref_block_num":1111,"ref_block_prefix":61170152,"expiration":"2016-03-24T18:01:03","operations":[{"type":"pow_operationx","value":{"worker_account":"steemit20","block_id":"00000457e861a5031091d4f023a0ea59940769da","nonce":158936,"work":{"worker":"STM65wH1LZ7BfSHcK69SShnqCAH5xdoSZpGkUjmzHJ5GCuxEK9V5G","input":"276060533b97694f0b7cec59d89e0457732e6356b56103eead518a4ec4b10abe","signature":"1f3328dd7fc9ffbcbbf838e4498b39ab22bc09327eccf4cf4e504b68b1eca2df336c8465ad38641639f114faeb4d357bea8dd3a9b898653c1eb259f75c5fc971fb","work":"0000117040ae389c5e52e5e8e0ac384f1c595ca98fcde7919b68ee7e8c73737a"},"props":{"account_creation_fee":{"amount":"100000","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}}],"extensions":[],"signatures":[]}'
result_01 = example.validate_transaction(trx_01)
print( f"{trx_01} = result: {result_01}" )
print("====================================================================")

trx_02 = b'{"ref_block_num":1111,"ref_block_prefix":61170152,"expiration":"2016-03-24T18:01:03","operations":[{"type":"pow_operation","value":{"worker_account":"steemit20","block_id":"00000457e861a5031091d4f023a0ea59940769da","nonce":158936,"work":{"worker":"STM65wH1LZ7BfSHcK69SShnqCAH5xdoSZpGkUjmzHJ5GCuxEK9V5G","input":"276060533b97694f0b7cec59d89e0457732e6356b56103eead518a4ec4b10abe","signature":"1f3328dd7fc9ffbcbbf838e4498b39ab22bc09327eccf4cf4e504b68b1eca2df336c8465ad38641639f114faeb4d357bea8dd3a9b898653c1eb259f75c5fc971fb","work":"0000117040ae389c5e52e5e8e0ac384f1c595ca98fcde7919b68ee7e8c73737a"},"props":{"account_creation_fee":{"amount":"0","precision":3,"nai":"@@000000021"},"maximum_block_size":131072,"hbd_interest_rate":1000}}}],"extensions":[],"signatures":[]}'
result_02 = example.validate_transaction(trx_02)
print( f"{trx_02} = result: {result_02}" )
print("====================================================================")