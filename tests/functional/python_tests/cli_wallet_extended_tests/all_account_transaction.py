import json

from test_tools.communication import request
from typing import Dict, List


develop_url = 'http://localhost:28091'
json_server_develop_data_stored = '/home/dev/Desktop/server_request_data/server_request_develop_data/server_request_develop_data'
develop_last_block_number = 4415

with open('/home/dev/Desktop/request data/request_master_data/response_master.json', 'r') as file:
    master_transactions = json.load(file)


def get_all_account_transactions(account_name: str, last_block_number, url) -> List[Dict]:
    all_account_transaction = []

    for i in range(last_block_number):
        request_data = request(url,
                               {"id": 9, "jsonrpc": "2.0", "method": "account_history_api.get_account_history",
                                "params": {"account": account_name, "start": last_block_number-i, "limit": 1}})
        all_account_transaction.append(request_data)
    return all_account_transaction


def save_all_transaction():
    store = get_all_account_transactions('gtg', develop_last_block_number, develop_url)
    with open(json_server_develop_data_stored, 'w') as file:
        json.dump(store, file)


dev_transactions = get_all_account_transactions('gtg', develop_last_block_number, develop_url)

print(dev_transactions[0]['result']['history'][0][1]['op'])
print(master_transactions[0]['result']['history'][0][1]['op'])

if dev_transactions[0]['result']['history'][0][1]['op'] == master_transactions[1]['result']['history'][0][1]['op']:
    print('takie różna')

# for index_dev, c in enumerate(dev_transactions):
#     for index_mas, l in enumerate(master_transactions):
#         if not dev_transactions[index_dev]['result']['history'][0][1]['op'] == master_transactions[index_mas]['result']['history'][0][1]['op']:
#             print(dev_transactions[index_dev]['result']['history'][0][1]['op'])
#

