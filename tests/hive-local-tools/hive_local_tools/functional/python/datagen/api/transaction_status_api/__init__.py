import json


def read_transaction_ids(directory: str) -> list:
    with open(f'{directory}/transactions_ids.json', 'r') as json_file:
        return json.load(json_file)


def assert_transaction_status(node, transaction_ids: str, reference_status: str):
    assert node.api.transaction_status.find_transaction(transaction_id=transaction_ids)['status'] == reference_status


def verify_transaction_status_in_block_range(node, first_block_num: int, second_block_num: int, transactions_ids: list, reference_status: str):
    for transaction_id in transactions_ids:
        if transaction_id['block_num'] in range(first_block_num, second_block_num):
            assert_transaction_status(node, transaction_id['transaction_id'], reference_status)
