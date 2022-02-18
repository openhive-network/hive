import json

from concurrent.futures import ThreadPoolExecutor
from test_tools.communication import request
from typing import Dict, List


develop_url = 'http://localhost:28091'
server_develop_url = 'http://hive-2:8091'
json_server_develop_data_stored = '/home/dev/Desktop/server_request_data/server_request_develop_data/server_request_develop_data.json'
develop_last_block_number = 2955480

server_master_url = 'http://hive-2:18091'
json_server_master_data_stored = '/home/dev/Desktop/server_request_data/server_request_master_data/server_request_master_data.json'
master_last_block_number = 2955470


def get_all_account_transactions(account_name: str, start_block_number, last_block_number, url) -> List[Dict]:
    all_account_transaction = []
    for i in range(start_block_number, last_block_number+1, 1000):
        if start_block_number == 0:
            request_data = request(url,
                                   {"id": 9, "jsonrpc": "2.0", "method": "account_history_api.get_account_history",
                                    "params": {"account": account_name, "start": 1, "limit": 1}})
            all_account_transaction.append(request_data)
            start_block_number += 1000
        elif start_block_number < 1000:
            request_data = request(url,
                                   {"id": 9, "jsonrpc": "2.0", "method": "account_history_api.get_account_history",
                                    "params": {"account": account_name, "start": start_block_number, "limit": start_block_number}})
            start_block_number += 1000 - start_block_number
            all_account_transaction.append(request_data)
        else:
            request_data = request(url,
                                   {"id": 9, "jsonrpc": "2.0", "method": "account_history_api.get_account_history",
                                    "params": {"account": account_name, "start": i+1000, "limit": 1000}})
            all_account_transaction.append(request_data)

    return all_account_transaction


x = get_all_account_transactions('gtg', 0, 2000, server_develop_url)
m = get_all_account_transactions('gtg', 555, 6000, server_develop_url)
y = get_all_account_transactions('gtg', 2000, 4000, server_develop_url)
z = get_all_account_transactions('gtg', 4000, 6000, server_develop_url)

w = get_all_account_transactions('gtg', 12000, 16000, server_develop_url)
print()


def save_transaction_to_json(data_stored, account_name, start_block_number, last_block_number, url):
    store = get_all_account_transactions(account_name, start_block_number, last_block_number, url)
    with open(data_stored, 'w') as file:
        json.dump(store, file)


save_transaction_to_json(json_server_develop_data_stored, 'gtg', 0, 12000, server_develop_url)



# def multi_saver():
#
#     tasks_list = []
#     executor = ThreadPoolExecutor(max_workers=4)
#     for i in range(1000, 10000, 1000):
#         tasks_list.append(executor.submit(get_all_account_transactions, 'gtg', i, 10000, server_develop_url))
#
#     for thread_number in tasks_list:
#         thread_number.result()
#
#
# multi_saver()
# save_transaction_to_json(json_server_develop_data_stored, 'gtg', 10000, server_develop_url)


# dev_transactions = get_all_account_transactions('gtg', 100, server_develop_url)
# mas_transactions = get_all_account_transactions('gtg', 100, server_master_url)


# for index_mas in master_transactions:
#     if index_mas['result']['history'][0][1]['op']['type'] == 'limit_order_cancel_operation':
#         wrong_transactions.append(index_mas)
# for index_dev in dev_transactions:
#     if index_dev['result']['history'][0][1]['op']['type'] == 'limit_order_cancel_operation' or index_dev['result']['history'][0][1]['op']['type'] == 'limit_order_cancelled_operation':
#         wrong_transactions.append(index_dev)


# for index_dev, c in enumerate(dev_transactions):
#     d = 0
#     for index_mas, l in enumerate(master_transactions):
#         x = c['result']['history'][0][1]['op']
#         y = l['result']['history'][0][1]['op']
#         x_id = c['result']['history'][0][1]['trx_id']
#         y_id = l['result']['history'][0][1]['trx_id']
#         if c['result']['history'][0][1]['trx_id'] == l['result']['history'][0][1]['trx_id']:
#             if c['result']['history'][0][1]['op'] != l['result']['history'][0][1]['op']:
#                 if c['result']['history'][0][1]['trx_id'] != '0000000000000000000000000000000000000000':
#                     wrong_transactions.append(c)
#                     # print(c['result']['history'][0][1]['trx_id'])

# x = wrong_transactions
# print()