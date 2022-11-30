import re
import test_tools as tt
from typing import List

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
    assert len(sub_networks) > 1

    current_idx = 0
    while current_idx < len(sub_networks) - 1:
        next_current_idx = current_idx + 1
        while next_current_idx < len(sub_networks):
            tt.logger.info(f"Sub network {current_idx} connected with {next_current_idx}")
            sub_networks[current_idx].connect_with(sub_networks[next_current_idx])
            next_current_idx += 1
        current_idx += 1

def disconnect_sub_networks(sub_networks : list):
    assert len(sub_networks) > 1

    current_idx = 0
    while current_idx < len(sub_networks) - 1:
        next_current_idx = current_idx + 1
        while next_current_idx < len(sub_networks):
            tt.logger.info(f"Sub network {current_idx} disconnected from {next_current_idx}")
            sub_networks[current_idx].disconnect_from(sub_networks[next_current_idx])
            next_current_idx += 1
        current_idx += 1

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
    info            = wallet.api.info()
    hb              = info['head_block_number']
    lib             = info['last_irreversible_block_num']
    current_witness = info['current_witness']
    tt.logger.info(f'network: \'{msg}\' head: {hb} lib: {lib} current witness: {current_witness}')
    return hb, lib

class fork_log:
    def __init__(self, name, wallet):
        self.name       = name
        self.collector  = []
        self.wallet     = wallet

    def append(self):
        self.collector.append( info(self.name, self.wallet) )

def wait(blocks, log : List[fork_log], api_node):
    for i in range(blocks):

        for current in log:
            current.append()

        api_node.wait_number_of_blocks(1)
        tt.logger.info(f'{i+1}/{blocks} blocks')


def final_block_the_same(method, data : list):
    current = None
    for item in data:
        if current is None:
            current = item
        else:
            if method( current ) != method( item ):
                return False
    return True

def lib_true_condition():
    return True

def lib_custom_condition(compared_item1, compared_item2):
    return get_last_irreversible_block_num(compared_item1) > compared_item2

def wait_for_final_block(witness_node, logs, data : list, allow_lib = True, lib_cond = lib_true_condition, allow_last_head = True):
    assert allow_lib or allow_last_head

    #Hard to say when all nodes would have the same HEAD's/LIB's. All nodes are connected together in common network
    #so sometimes one or two nodes are "delayed" - their LIB is lower than LIB others.
    #The best option is to wait until every node has the same data: it doesn't matter if such situation occurs after 5 or 25 blocks.
    #In case when nodes wouldn't have the same data, it's an obvious error and CI will finish this test.
    while True:
        wait(1, logs, witness_node)

        #Veryfing if all nodes have the same last irreversible block number
        if allow_lib:
            if lib_cond() and final_block_the_same(get_last_irreversible_block_num, data):
                return False

        #Veryfing if all nodes have the same last head block number
        if allow_last_head:
            if final_block_the_same(get_last_head_block_number, data):
                return False

def calculate_transformed_witnesses(wallet, node):
    witnesses       = wallet.api.get_active_witnesses(False)['witnesses']
    current_witness = node.get_current_witness()
    tt.logger.info(f'current witness: {current_witness} active witnesses: {witnesses}')

    transformed_witnesses = []
    start = False
    pos = 0
    for witness in witnesses:
        if witness == current_witness:
            start = True
            continue
        if start:
            transformed_witnesses.insert(pos, witness)
            pos += 1
        else:
            transformed_witnesses.append(witness)

    tt.logger.info(f'transformed witnesses: {transformed_witnesses}')
    return transformed_witnesses

def is_witness_in_given_patterns(witness, witness_name_patterns):
    return any([re.match(pattern, witness) is not None for pattern in witness_name_patterns])

def are_witnesses_match_patterns(witnesses, witness_name_patterns):
    return all([is_witness_in_given_patterns(witness, pattern) for pattern, witness in zip(witness_name_patterns, witnesses)])

def wait_for_specific_witnesses(node, logs, witness_name_patterns):
    wallet = tt.Wallet(attach_to=node)
    witnesses_interval = 21
    while True:
        wait(1, logs, node)

        witnesses = calculate_transformed_witnesses(wallet, node)

        if are_witnesses_match_patterns(witnesses, witness_name_patterns):
            last_block_number   = node.get_last_block_number()
            val_1               = last_block_number % witnesses_interval
            val_2               = (last_block_number + len(witness_name_patterns)) % witnesses_interval
            tt.logger.info(f'schedule-status now: {val_1} schedule-status after: {val_2}')
            if val_1 < val_2:
                tt.logger.info("Witnesses patterns will be processed in the same schedule")
                return
            else:
                tt.logger.info("Witnesses patterns can't be processed in the same schedule. Still waiting...")
