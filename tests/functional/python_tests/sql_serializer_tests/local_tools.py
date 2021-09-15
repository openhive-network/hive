from test_tools import logger, Account
import time
import threading


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
