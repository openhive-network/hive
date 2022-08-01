import test_tools as tt
from tests.api_tests.message_format_tests.wallet_bridge_api_tests.conftest import wallet

from .local_tools import connect_sub_networks, disconnect_sub_networks

import test_tools as tt

def get_last_head_block_number(blocks : list):
    return blocks[len(blocks) - 1][0]

def get_last_irreversible_block_num(blocks : list):
    return blocks[len(blocks) - 1][1]

def info(msg : str, wallet : tt.Wallet):
    info    = wallet.api.info()
    hb      = info['head_block_number']
    lib     = info['last_irreversible_block_num']
    tt.logger.info(f'msg: {msg} head_block_number: {hb} last_irreversible_block_num: {lib}')
    return hb, lib

def test_fast_confirmation_fork(prepared_sub_networks_18_3):
    sub_networks = prepared_sub_networks_18_3['sub-networks']
    assert len(sub_networks) == 2

    majority_api_node = sub_networks[0].node('ApiNode0')
    minority_api_node = sub_networks[1].node('ApiNode1')

    majority_wallet = tt.Wallet(attach_to = majority_api_node)
    minority_wallet = tt.Wallet(attach_to = minority_api_node)

    majority_blocks = []
    minority_blocks = []

    majority_blocks.append( info('M', majority_wallet) )
    minority_blocks.append( info('m', minority_wallet) )

    assert get_last_head_block_number(majority_blocks)      == get_last_head_block_number(minority_blocks)
    assert get_last_irreversible_block_num(majority_blocks) == get_last_irreversible_block_num(minority_blocks)

    disconnect_sub_networks(sub_networks)

    for i in range(22):
        majority_blocks.append( info('M', majority_wallet) )
        minority_blocks.append( info('m', minority_wallet) )
        majority_api_node.wait_number_of_blocks(1)

    assert get_last_head_block_number(majority_blocks)      > get_last_head_block_number(minority_blocks)
    assert get_last_irreversible_block_num(majority_blocks) > get_last_irreversible_block_num(minority_blocks)

    connect_sub_networks(sub_networks)

    for i in range(22):
        majority_blocks.append( info('M', majority_wallet) )
        minority_blocks.append( info('m', minority_wallet) )
        majority_api_node.wait_number_of_blocks(1)

    assert get_last_head_block_number(majority_blocks)      == get_last_head_block_number(minority_blocks)
    assert get_last_irreversible_block_num(majority_blocks) == get_last_irreversible_block_num(minority_blocks)