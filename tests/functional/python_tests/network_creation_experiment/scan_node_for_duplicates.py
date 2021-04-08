import argparse
import requests
import json


def get_reward_operations(ops):
    result = []
    for op in ops:
        op_type = op["op"]["type"]
        if op_type == "producer_reward_operation":
            result.append(op)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('address', metavar='add', nargs=1, type=str, help="ip::port of node")
    parser.add_argument('start', metavar='start', nargs=1, type=int, help="start block")
    parser.add_argument('end', metavar='end', nargs=1, type=int, help="end block")
    
    args = parser.parse_args()

    print('address: ', args.address)
    url = f'http://' + args.address[0]
    print('start: ', args.start)
    start = args.start[0]
    print('end: ', args.end)
    end = args.end[0]

    blocks_with_duplicates = []

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
        print(f'int block {i}  there is {length} ops', end='')
        reward_operations = get_reward_operations(ops)
        size = len(reward_operations)
        print(f'including {size} producer reward operations')

        if(size>1):
            blocks_with_duplicates.append(i)


    if len(blocks_with_duplicates) == 0:
        print('THERE ARE NO DUPLICATES')
    else:
        print('duplicates:')
        prit(blocks_with_duplicates)

