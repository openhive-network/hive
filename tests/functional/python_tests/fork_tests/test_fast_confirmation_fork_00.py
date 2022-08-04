import test_tools as tt
from tests.api_tests.message_format_tests.wallet_bridge_api_tests.conftest import wallet

from .local_tools import connect_sub_networks, disconnect_sub_networks
from .local_tools import get_last_head_block_number, get_last_irreversible_block_num, info, wait
from .local_tools import assert_no_duplicates

import test_tools as tt

def test_fast_confirmation_fork_00(prepared_sub_networks_3_18):
    # start - A network (consists of a 'minority' network(3 witnesses) + a 'majority' network(18 witnesses) produces blocks

    # - the network is splitted into 2 sub networks: 6 witnesses(the 'minority' network) + 18 witnesses(the 'majority' network)
    # - wait '10' blocks( using the majority API node ) - as a result a chain of blocks in the 'majority' network is longer than the 'minority' network
    # - 2 sub networks are merged
    # - wait 'N' blocks( using the majority API node ) until both sub networks have the same last irreversible block

    # Finally the 'minority' network received blocks from the 'majority' network

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
    last_head_block_majority                                = get_last_head_block_number(majority_blocks)

    old_majority_last_lib = get_last_irreversible_block_num(majority_blocks)
    tt.logger.info(f'Reconnect sub networks - start')
    connect_sub_networks(sub_networks)
    while True:
        wait(1, majority_blocks, majority_wallet, minority_blocks, minority_wallet, majority_api_node)

        majority_last_lib = get_last_irreversible_block_num(majority_blocks)

        assert old_majority_last_lib + 1 == majority_last_lib
        old_majority_last_lib = majority_last_lib

        if majority_last_lib == get_last_irreversible_block_num(minority_blocks):
            break

    assert_no_duplicates(minority_api_node, majority_api_node)