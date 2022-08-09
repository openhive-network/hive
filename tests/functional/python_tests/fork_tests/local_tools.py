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


def connect_sub_networks(sub_networks : list):
    if len(sub_networks) > 1:
        current_sub_network = None
        cnt = -1
        for sub_network in sub_networks:
            cnt += 1
            if current_sub_network is None:
                current_sub_network = sub_network
                continue
            else:
                assert (current_sub_network is not None) and (sub_network is not None)
                tt.logger.info(f"Sub network {cnt} connected with {cnt-1}")
                current_sub_network.connect_with(sub_network)
                current_sub_network = sub_network

def disconnect_sub_networks(sub_networks : list):
    if len(sub_networks) > 1:
        current_sub_network = None
        cnt = -1
        for sub_network in sub_networks:
            cnt += 1
            if current_sub_network is None:
                current_sub_network = sub_network
                continue
            else:
                assert (current_sub_network is not None) and (sub_network is not None)
                tt.logger.info(f"Sub network {cnt} disconnected from {cnt-1}")
                current_sub_network.disconnect_from(sub_network)
                current_sub_network = sub_network

def enable_witnesses(wallet : tt.Wallet, witness_details : list):
    with wallet.in_single_transaction():
        for name in witness_details:
            wallet.api.update_witness(
                name, "https://" + name,
                tt.Account(name).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

def disable_witnesses(wallet : tt.Wallet, witness_details : list):
    key = 'TST5NUU7M7pmqMpMHUwscgUBMuwLQE56MYwCLF7q9ZGB6po1DMNoG'
    with wallet.in_single_transaction():
        for name in witness_details:
            wallet.api.update_witness(
                name, "https://" + name,
                key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

def get_last_head_block_number(blocks : list):
    return blocks[len(blocks) - 1][0]

def get_last_irreversible_block_num(blocks : list):
    return blocks[len(blocks) - 1][1]

def get_part_of_witness_details(witness_details : list, start, length : int):
    assert start >= 0 and start + length <= len(witness_details)
    new_witness_details = []
    for i in range(start, start + length):
        new_witness_details.append(witness_details[i])
    return new_witness_details

def info(msg : str, wallet : tt.Wallet):
    info    = wallet.api.info()
    hb      = info['head_block_number']
    lib     = info['last_irreversible_block_num']
    tt.logger.info(f'msg: {msg} head_block_number: {hb} last_irreversible_block_num: {lib}')
    return hb, lib

class fork_log:
    def __init__(self, name, wallet):
        self.name       = name
        self.collector  = []
        self.wallet     = wallet

def append(log1 : fork_log, log3 : fork_log, log2 : fork_log):
    log1.collector.append( info(log1.name, log1.wallet) )
    log1.collector.append( info(log2.name, log2.wallet) )
    log1.collector.append( info(log3.name, log3.wallet) )

def wait_v2(blocks, log1 : fork_log, log3 : fork_log, log2 : fork_log, api_node):
    for i in range(blocks):
        append(log1, log2, log3)
        api_node.wait_number_of_blocks(1)
        tt.logger.info(f'{i+1}/{blocks} blocks')

def wait(blocks, majority_blocks, majority_wallet, minority_blocks, minority_wallet, majority_api_node):
    for i in range(blocks):
        majority_blocks.append( info('M', majority_wallet) )
        minority_blocks.append( info('m', minority_wallet) )
        majority_api_node.wait_number_of_blocks(1)
        tt.logger.info(f'{i+1}/{blocks} blocks')
