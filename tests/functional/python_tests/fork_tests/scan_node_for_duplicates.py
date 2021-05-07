import argparse
import requests
import json


def get_producer_reward_operations(ops):
    result = []
    for op in ops:
        op_type = op["op"]["type"]
        if op_type == "producer_reward_operation":
            result.append(op)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('address', metavar='add', nargs=1, type=str, help="ip::port of node")
    parser.add_argument('--start', type=int, help="start block", default=1)
    parser.add_argument('--end', type=int, help="end block", default=1000)
    
    args = parser.parse_args()

    print('address: ', args.address)
    url = f'http://' + args.address[0]
    print('start: ', args.start)
    start = args.start
    print('end: ', args.end)
    end = args.end

    blocks_with_duplicates = []
    blocks_with_no_operations = []

    for i in range(start, end):
        
        dict_data = {
            "jsonrpc": "2.0",
            "method": "call",
            "params": [
                "account_history_api",
                "get_ops_in_block",
                {
                    "block_num": i,
                    "only_virtual": True
                }
            ],
            "id": 1
        }
        json_data = message = bytes(json.dumps(dict_data), "utf-8") + b"\r\n"
        result = requests.post(url, data=message)
        response = json.loads(result.text)
        ops = response["result"]["ops"]
        length = len(ops)
        reward_operations = get_producer_reward_operations(ops)
        size = len(reward_operations)

        if(size>1):
            blocks_with_duplicates.append(i)
        if(size==0):
            blocks_with_no_operations.append(i)


    if len(blocks_with_duplicates) == 0:
        print('THERE ARE NO DUPLICATES')
    else:
        print('duplicates:')
        print(blocks_with_duplicates)


    if len(blocks_with_no_operations) == 0:
        print('THERE ARE NO EMPTY BLOCKS')
    else:
        print('empty blocks:')
        print(blocks_with_no_operations)
