from test_tools import logger, Account
import time
import threading


def count_ops_by_type(node, op_type: str, start: int, limit: int = 50):
    """
    :param op_type: type of operation (ex. 'producer_reward_operation')
    :param start: start queries with this block number
    :param limit: limit queries until start-limit+1
    """
    count = {}
    for i in range(start, start-limit, -1):
        response = node.api.account_history.get_ops_in_block(block_num=i, only_virtual=False)
        ops = response["result"]["ops"]
        count[i] = 0
        for op in ops:
            this_op_type = op["op"]["type"]
            if this_op_type == op_type:
                count[i] += 1
    return count


def check_account_history_duplicates(node, wallet):
    last_irreversible_block = wallet.api.info()["result"]["last_irreversible_block_num"]
    node_reward_operations = count_ops_by_type(node, 'producer_reward_operation', last_irreversible_block, limit=50)
    assert sum(i==1 for i in node_reward_operations.values()) == 50


def assert_no_duplicates(node, *nodes):
    nodes = [node, *nodes]
    for node in nodes:
        wallet = node.attach_wallet()
        check_account_history_duplicates(node, wallet)
    node.wait_number_of_blocks(10)
    for node in nodes:
        wallet = node.attach_wallet()
        check_account_history_duplicates(node, wallet)
    logger.info("No there are no duplicates in account_history.get_ops_in_block...")


def generate(debug_node, block_interval):
    global timestamp
    time.sleep(block_interval)
    response = debug_node.api.debug_node.debug_get_scheduled_witness( slot = 1 )
    scheduled_witness = response['result']['scheduled_witness']
    logger.info(f'{debug_node}: will generate block with {scheduled_witness} key')
    debug_node.api.debug_node.debug_generate_blocks(
        debug_key=Account(scheduled_witness).private_key,
        count = 1,
        skip = 0,
        miss_blocks = 0,
        edit_if_needed = False,
        timestamp = debug_node.timestamp or 0,
        broadcast = True
    )
    if debug_node.timestamp:
        debug_node.timestamp += 3

class ProducingBlocksInBackgroundContext:
    def __init__(self, debug_node, block_interval):
        self.__debug_node = debug_node
        self.__block_interval = block_interval
        self.__running = False

    def __enter__(self):
        self.__thread = threading.Thread(target = self.__generating_loop)
        self.__running = True
        self.__thread.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.__running = False
        self.__thread.join()

    def __generating_loop(self):
        while self.__running:
            generate(self.__debug_node, self.__block_interval)
