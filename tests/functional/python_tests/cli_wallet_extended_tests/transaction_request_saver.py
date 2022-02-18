import json

from test_tools.communication import request
from typing import Dict, List


master_url = 'http://localhost:18091'
server_master_url = 'http://hive-2:18091'
master_data_stored = '/home/dev/Desktop/request data/request_master_data/response_master.txt'
server_master_data_stored = 'http://hive-2:8091'
json_master_data_stored = '/home/dev/Desktop/request data/request_master_data/response_master.json'
json_server_master_data_stored = '/home/dev/Desktop/server_request_data/server_request_master_data.json'

master_last_block_number = 4409

develop_url = 'http://localhost:28091'
develop_data_stored = '/home/dev/Desktop/request data/request_develop_data/response_develop.txt'
json_develop_data_stored = '/home/dev/Desktop/request data/request_develop_data/response_develop.json'
json_server_develop_data_stored = '/home/dev/Desktop/server_request_data/server_request_develop_data/server_request_develop_data'
develop_last_block_number = 4415

duplicate_trx = [
                '486b5caec9fd094ec21144c67ecef12aa15c8f4f',
                'ec9bc6087e2a0147d4065f13f9a4c76fc1bde230',
                '002eb462739f65940d0ec349aa3ef0fb9f02940c',
                '37baf4a3221b736e676436ce26ce2ddb641a6f35',
                '05d3e11da3546696f2f9dc080b8b45a342153b94',
                '9acdc8e6524718d389337f94cd00cd7b8fd9ea65',
                ]

trx_id_develop = []
trx_id_master = []


def get_all_account_transactions(account_name: str, last_block_number, url) -> List[Dict]:
    all_account_transaction = []

    for i in range(last_block_number):
        request_data = request(url,
                               {"id": 9, "jsonrpc": "2.0", "method": "account_history_api.get_account_history",
                                "params": {"account": account_name, "start": last_block_number-i, "limit": 1}})
        all_account_transaction.append(request_data)
    return all_account_transaction


def find_wrong_transaction(wrong_trx_id):

    all_account_transaction = get_all_account_transactions('gtg', develop_last_block_number, develop_url)
    wrong_transaction_to_file = []

    for i in wrong_trx_id:
        for j in all_account_transaction:
            if i == j['result']['history'][0][1]['trx_id']:
                wrong_transaction_to_file.append(j)
                print(j)

    with open('/home/dev/Desktop/request data/wrong_transaction.json', 'w') as wrong_file:
        wrong_file.write(f'{wrong_transaction_to_file}')


def transaction_request_saver(url, data_place, last_block_number, json_file):

    string_data_pack = []

    def tx_id_saver():
        with open('/home/dev/Desktop/request data/request_master_data/trx_id_master.txt', 'w') as trx_txt:
            for _ in trx_id_master:
                trx_txt.write(f'{_}\n')
        with open('/home/dev/Desktop/request data/request_develop_data/trx_id_develop.txt', 'w') as trx_txt:
            for _ in trx_id_develop:
                trx_txt.write(f'{_}\n')

    def transaction_comparator():

        if url == master_url:
            trx_id_master.append(request_data['result']['history'][0][1]['trx_id'])
        else:
            trx_id_develop.append(request_data['result']['history'][0][1]['trx_id'])

    for i in range(last_block_number):
        request_data = request(url,
                               {"id": 9, "jsonrpc": "2.0", "method": "account_history_api.get_account_history",
                                "params": {"account": "gtg", "start": last_block_number-i, "limit": 1}})
        string_data_pack.append(request_data)
        transaction_comparator()
    tx_id_saver()

    with open(data_place, 'w') as txt_file:
        txt_file.write(f'{string_data_pack}\n')

    with open(json_file, 'w') as json_file:
        json.dump(string_data_pack, json_file)


get_all_account_transactions('gtg', 100, develop_url)

# master_dict = get_all_account_transactions('gtg', master_last_block_number, master_url)
# develop_dict = get_all_account_transactions('gtg', develop_last_block_number, develop_url)

# transaction_request_saver(master_url, master_data_stored, master_last_block_number, json_master_data_stored)
# transaction_request_saver(develop_url, develop_data_stored, develop_last_block_number, json_develop_data_stored)
#
# trx_id_difference_master_to_develop = [diff for diff in trx_id_master if diff not in trx_id_develop]
# trx_id_difference_develop_to_master = [diff for diff in trx_id_develop if diff not in trx_id_master]
# print('len trx_id_master:  ', len(trx_id_master))
# print('list difference master - develop: ', trx_id_difference_master_to_develop)
# print('len trx_id_develop: ', len(trx_id_develop))
# print('list difference develop - master: ', trx_id_difference_develop_to_master)
