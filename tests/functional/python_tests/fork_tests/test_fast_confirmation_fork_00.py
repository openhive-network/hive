import test_tools as tt
from tests.api_tests.message_format_tests.wallet_bridge_api_tests.conftest import wallet

from .local_tools import connect_sub_networks, disconnect_sub_networks
from .local_tools import get_last_head_block_number, get_last_irreversible_block_num, info, wait
from .local_tools import assert_no_duplicates

import test_tools as tt

def test_fast_confirmation_fork_00(prepared_sub_networks_3_18):
    sub_networks_data   = prepared_sub_networks_3_18['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    witness_details     = sub_networks_data[1]
    assert len(sub_networks) == 2

    minority_api_node = sub_networks[0].node('ApiNode0')
    majority_api_node = sub_networks[1].node('ApiNode1')

    majority_wallet = tt.Wallet(attach_to = majority_api_node)
    minority_wallet = tt.Wallet(attach_to = minority_api_node)

    majority_blocks = []
    minority_blocks = []

    majority_blocks.append( info('M', majority_wallet) )
    minority_blocks.append( info('m', minority_wallet) )

    assert get_last_head_block_number(majority_blocks)      == get_last_head_block_number(minority_blocks)
    assert get_last_irreversible_block_num(majority_blocks) == get_last_irreversible_block_num(minority_blocks)

    tt.logger.info(f'Disconnect sub networks - start')
    disconnect_sub_networks(sub_networks)
    wait(10, majority_blocks, majority_wallet, minority_blocks, minority_wallet, majority_api_node)

    assert get_last_head_block_number(majority_blocks)      > get_last_head_block_number(minority_blocks)
    assert get_last_irreversible_block_num(majority_blocks) > get_last_irreversible_block_num(minority_blocks)

    tt.logger.info(f'Reconnect sub networks - start')
    connect_sub_networks(sub_networks)
    wait(10, majority_blocks, majority_wallet, minority_blocks, minority_wallet, majority_api_node)

    assert get_last_head_block_number(majority_blocks)      == get_last_head_block_number(minority_blocks)
    assert get_last_irreversible_block_num(majority_blocks) == get_last_irreversible_block_num(minority_blocks)

    assert_no_duplicates(minority_api_node, majority_api_node)