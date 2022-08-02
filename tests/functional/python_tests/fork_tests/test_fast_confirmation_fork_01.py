import test_tools as tt
from tests.api_tests.message_format_tests.wallet_bridge_api_tests.conftest import wallet

from .local_tools import connect_sub_networks, disconnect_sub_networks, enable_witnesses, disable_witnesses
from .local_tools import get_last_head_block_number, get_last_irreversible_block_num, get_part_of_witness_details, info, wait
from .local_tools import assert_no_duplicates

import test_tools as tt

def test_fast_confirmation_fork_01(prepared_sub_networks_6_17):
    sub_networks_data   = prepared_sub_networks_6_17['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    assert len(sub_networks) == 2

    witness_details     = sub_networks_data[1]
    witness_details_part  = get_part_of_witness_details(witness_details, 6, 14)

    init_wallet         = sub_networks_data[2]

    minority_api_node   = sub_networks[0].node('ApiNode0')
    majority_api_node   = sub_networks[1].node('ApiNode1')

    majority_wallet = tt.Wallet(attach_to = majority_api_node)
    minority_wallet = tt.Wallet(attach_to = minority_api_node)

    majority_blocks = []
    minority_blocks = []

    majority_blocks.append( info('M', majority_wallet) )
    minority_blocks.append( info('m', minority_wallet) )

    assert get_last_head_block_number(majority_blocks)      == get_last_head_block_number(minority_blocks)
    assert get_last_irreversible_block_num(majority_blocks) == get_last_irreversible_block_num(minority_blocks)

    tt.logger.info(f'Disable {len(witness_details_part)} witnesses - start')
    disable_witnesses(init_wallet, witness_details_part)
    wait(3, majority_blocks, majority_wallet, minority_blocks, minority_wallet, minority_api_node)

    tt.logger.info(f'Disconnect sub networks - start')
    disconnect_sub_networks(sub_networks)
    wait(20, majority_blocks, majority_wallet, minority_blocks, minority_wallet, minority_api_node)

    tt.logger.info(f'Enable {len(witness_details_part)} witnesses')
    enable_witnesses(init_wallet, witness_details_part)
    wait(5, majority_blocks, majority_wallet, minority_blocks, minority_wallet, minority_api_node)

    tt.logger.info(f'Reconnect sub networks')
    connect_sub_networks(sub_networks)
    wait(5, majority_blocks, majority_wallet, minority_blocks, minority_wallet, majority_api_node)

    assert_no_duplicates(minority_api_node, majority_api_node)