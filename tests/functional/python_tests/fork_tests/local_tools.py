from datetime import datetime, timezone

import test_tools as tt


def count_ops_by_type(node, op_type: str, start: int, limit: int = 50):
    """
    :param op_type: type of operation (ex. 'producer_reward_operation')
    :param start: start queries with this block number
    :param limit: limit queries until start-limit+1
    """
    count = {}
    for i in range(start, start-limit, -1):
        response = node.api.account_history.get_ops_in_block(block_num=i, only_virtual=False)
        ops = response["ops"]
        count[i] = 0
        for op in ops:
            this_op_type = op["op"]["type"]
            if this_op_type == op_type:
                count[i] += 1
    return count


def check_account_history_duplicates(node, wallet):
    last_irreversible_block = wallet.api.info()["last_irreversible_block_num"]
    node_reward_operations = count_ops_by_type(node, 'producer_reward_operation', last_irreversible_block, limit=50)
    assert sum(i==1 for i in node_reward_operations.values()) == 50


def assert_no_duplicates(node, *nodes):
    nodes = [node, *nodes]
    for node in nodes:
        wallet = tt.Wallet(attach_to=node)
        check_account_history_duplicates(node, wallet)
    node.wait_number_of_blocks(10)
    for node in nodes:
        wallet = tt.Wallet(attach_to=node)
        check_account_history_duplicates(node, wallet)
    tt.logger.info("No there are no duplicates in account_history.get_ops_in_block...")


def get_time_offset_from_iso_8601(timestamp: str) -> str:
    current_time = datetime.now(timezone.utc)
    new_time = datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%S').replace(tzinfo=timezone.utc)

    # reduce node start delay from 10s, caused test fails
    difference = round(new_time.timestamp()-current_time.timestamp()) - 5
    time_offset = f'{difference}s'
    return time_offset
