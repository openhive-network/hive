from datetime import datetime, timezone

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


def get_time_offset_from_iso_8601(timestamp: str) -> str:
    current_time = datetime.now(timezone.utc)
    new_time = datetime.strptime(timestamp, '%Y-%m-%dT%H:%M:%S').replace(tzinfo=timezone.utc)

    # reduce node start delay from 10s, caused test fails
    difference = round(new_time.timestamp()-current_time.timestamp()) - 5
    time_offset = f'{difference}s'
    return time_offset

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
    info    = wallet.api.info()
    hb      = info['head_block_number']
    lib     = info['last_irreversible_block_num']
    tt.logger.info(f'network: \'{msg}\' head: {hb} lib: {lib}')
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
