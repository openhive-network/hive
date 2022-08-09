# import test_tools as tt
# from tests.api_tests.message_format_tests.wallet_bridge_api_tests.conftest import wallet

# from .local_tools import connect_sub_networks, disconnect_sub_networks, fork_log
# from .local_tools import get_last_head_block_number, get_last_irreversible_block_num, info, wait, append, fork_log
# from .local_tools import assert_no_duplicates

# import test_tools as tt

# def test_fast_confirmation_fork_02(prepared_sub_networks_1_3_17):
#     # start - A network (consists of a 'minority_1' network(1 witness) + consists of a 'minority_3' network(3 witnesses) + a 'majority' network(17 witnesses) produces blocks

#     # - the network is splitted into 3 sub networks: 1 witness(the 'minority_1' network) + 3 witnesses(the 'minority_3' network) + 17 witnesses(the 'majority' network)
#     # - wait '10' blocks( using the majority API node ) - as a result a chain of blocks in the 'majority' network is longer than the both 'minority' networks
#     # - 3 sub networks are merged
#     # - wait 'N' blocks( using the majority API node ) until both sub networks have the same last irreversible block

#     # Finally the both 'minority' networks received blocks from the 'majority' network

#     sub_networks_data   = prepared_sub_networks_1_3_17['sub-networks-data']
#     sub_networks        = sub_networks_data[0]
#     assert len(sub_networks) == 3

#     minority_1_api_node = sub_networks[0].node('ApiNode0')
#     minority_3_api_node = sub_networks[1].node('ApiNode1')
#     majority_api_node   = sub_networks[2].node('ApiNode2')


#     majority_log     = fork_log("M  ", tt.Wallet(attach_to = majority_api_node))
#     minority_1_log   = fork_log("m-1", tt.Wallet(attach_to = minority_1_api_node))
#     minority_3_log   = fork_log("m-3", tt.Wallet(attach_to = minority_3_api_node))

#     append(majority_log, minority_1_log, minority_3_log)

#     tt.logger.info(f'Disconnect sub networks - start')
#     disconnect_sub_networks(sub_networks)
#     wait(10, majority_log, minority_1_log, minority_3_log, majority_api_node)

#     tt.logger.info(f'Reconnect sub networks - start')
#     connect_sub_networks(sub_networks)
#     wait(10, majority_log, minority_1_log, minority_3_log, majority_api_node)

#     assert_no_duplicates(minority_1_api_node, majority_api_node)
#     assert_no_duplicates(minority_3_api_node, majority_api_node)
