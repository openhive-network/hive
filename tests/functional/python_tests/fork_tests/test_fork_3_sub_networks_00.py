from .local_tools import wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num
import test_tools as tt

def test_fork_3_sub_networks_00(prepared_sub_networks_1_3_17):
    # start - A network (consists of a 'minority_1' network(1 witness) + consists of a 'minority_3' network(3 witnesses) + a 'majority' network(17 witnesses) produces blocks

    # - the network is splitted into 3 sub networks: 1 witness(the 'minority_1' network) + 3 witnesses(the 'minority_3' network) + 17 witnesses(the 'majority' network)
    # - wait '10' blocks( using the majority API node ) - as a result a chain of blocks in the 'majority' network is longer than the both 'minority' networks
    # - 3 sub networks are merged
    # - wait 'N' blocks( using the majority API node ) until both sub networks have the same last irreversible block

    # Finally the both 'minority' networks received blocks from the 'majority' network




#Not finished - now 3 sub networks and more aren't supported in test_tools