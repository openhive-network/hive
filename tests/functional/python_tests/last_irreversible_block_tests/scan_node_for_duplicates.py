import argparse
import requests
import json
import time


def get_lib(url):
    properties_query_data = {
        "jsonrpc": "2.0",
        "method": "call",
        "params": [
            "database_api",
            "get_dynamic_global_properties",
            {}
        ],
        "id": 1
    }

    json_data = bytes(json.dumps(properties_query_data), "utf-8") + b"\r\n"
    result = requests.post(url, data=json_data)
    response = json.loads(result.text)
    last_irreversible_block = response["result"]["last_irreversible_block_num"]
    return last_irreversible_block


def get_ops(url, block_num):
    ops_query_data = {
        "jsonrpc": "2.0",
        "method": "call",
        "params": [
            "account_history_api",
            "get_ops_in_block",
            {
                "block_num": block_num,
                "only_virtual": False
            }
        ],
        "id": 1
    }
    json_data = bytes(json.dumps(ops_query_data), "utf-8") + b"\r\n"
    result = requests.post(url, data=json_data)
    response = json.loads(result.text)
    ops = response["result"]["ops"]
    return ops

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('url', metavar='add', nargs=1, type=str, help="endpoint protocol://ip:port")
    
    args = parser.parse_args()

    url = args.url[0]
    print('url: ', url)

    start_lib = get_lib(url)
    scanned_until = start_lib
    last_irreversible_block = start_lib
    
    while True:
        while True:
            last_irreversible_block = get_lib(url)
            print("last_irreversible_block: " + str(last_irreversible_block))

            if last_irreversible_block == scanned_until:
                time.sleep(1)
            else:
                break

        for block_num in range(scanned_until+1, last_irreversible_block+1): 
            ops = get_ops(url, block_num)

            duplicates_count = 0
            for j in range(len(ops)):
                for i in range(j):
                    op1 = ops[i]
                    op2 = ops[j]
                    if op1==op2:
                        duplicates_count += 1
                        print(f"FOUND DUPLICATES IN BLOCK {block_num}")
                        print(op1)
                        print(op2)

            if duplicates_count == 0:
                print(f"NO DUPLICATES IN BLOCK {block_num}")
        
        scanned_until = last_irreversible_block

