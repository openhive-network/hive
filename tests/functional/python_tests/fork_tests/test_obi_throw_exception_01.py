from .local_tools import wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num
import test_tools as tt
import time

def test_obi_throw_exception_01(prepare_obi_throw_exception_01):
    # start - A network (consists of a 'A' network(10 witnesses) + a 'B' network(11 witnesses)) produces blocks

    # A witness from the `A` network has an exception - during 'sleep_time_in_sec' seconds this sub network can't produce.
    # After production resuming, both sub networks can't link together and LIB is still the same.

    #=====================================================================================================================================
    # **********witness 'w0' ('A' network)**********

    # {"num":120,"lib":119,"type":"p2p","id":"00000078631a5ea8153e9d04f6dabd5f436272e5","bp":"witness-1-2"}
    # {"num":121,"lib":120,"type":"gen","id":"0000007956e004637532d22574c55fad6bd570b8","bp":"witness-0-4"}

    # **********start an artificial exception**********

    # {"num":122,"lib":121,"type":"gen","id":"0000007a858ffa840f1e66690a1db2384819567a","bp":"witness-0-1"}

    # **********end of an artificial exception**********

    # {"num":122,"lib":121,"type":"gen","id":"0000007aa16fbde9a5a046e718f83115e0899841","bp":"witness-0-7"}
    # {"num":123,"lib":121,"type":"p2p","id":"0000007b93955ddcaec73914246283bc930c85e1","bp":"witness-1-1"}

    # block does not link to known chain
    #=====================================================================================================================================
    # **********witness 'w1' ('B' network)**********

    # {"num":120,"lib":119,"type":"gen","id":"00000078631a5ea8153e9d04f6dabd5f436272e5","bp":"witness-1-2"}
    # {"num":121,"lib":120,"type":"p2p","id":"0000007956e004637532d22574c55fad6bd570b8","bp":"witness-0-4"}
    # {"num":122,"lib":121,"type":"p2p","id":"0000007a858ffa840f1e66690a1db2384819567a","bp":"witness-0-1"}

    # **********Encountered block num collision at block 122 due to a fork, witnesses are: [["witness-0-1"],["witness-0-7"]]

    # {"num":122,"lib":121,"type":"ignored","id":"0000007aa16fbde9a5a046e718f83115e0899841","bp":"witness-0-7"}
    # {"num":123,"lib":121,"type":"gen","id":"0000007b93955ddcaec73914246283bc930c85e1","bp":"witness-1-1"}
    #=====================================================================================================================================

    sub_networks_data   = prepare_obi_throw_exception_01['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    assert len(sub_networks) == 2

    api_node_0      = sub_networks[0].node('ApiNode0')
    witness_node_0  = sub_networks[0].node('WitnessNode0')

    api_node_1      = sub_networks[1].node('ApiNode1')
    witness_node_1  = sub_networks[1].node('WitnessNode1')

    logs = []

    logs.append(fork_log("a0", tt.Wallet(attach_to = api_node_0)))
    logs.append(fork_log("w0", tt.Wallet(attach_to = witness_node_0)))

    logs.append(fork_log("a1", tt.Wallet(attach_to = api_node_1)))
    logs.append(fork_log("w1", tt.Wallet(attach_to = witness_node_1)))

    last_block_before_exception = 121
    blocks_after_exception      = 20
    delay_seconds               = 5

    _a0 = logs[0].collector

    tt.logger.info(f'Before an exception')

    while True:
        wait(1, logs, witness_node_0)
        if witness_node_0.get_last_block_number() >= last_block_before_exception:
            break

    tt.logger.info(f'Artificial exception is thrown during {delay_seconds} seconds')
    witness_node_0.api.debug_node.debug_throw_exception(throw_exception = True)

    time.sleep(delay_seconds)

    tt.logger.info(f'Artificial exception is disabled')
    witness_node_0.api.debug_node.debug_throw_exception(throw_exception = False)

    wait(blocks_after_exception, logs, witness_node_0)

    _a0 = logs[0].collector
    _w0 = logs[1].collector
    _a1 = logs[2].collector
    _w1 = logs[3].collector

    assert get_last_head_block_number(_a0) == get_last_head_block_number(_a1)
    assert get_last_head_block_number(_w0) < get_last_head_block_number(_w1)
    assert get_last_head_block_number(_a0) > get_last_head_block_number(_w0)

    assert get_last_irreversible_block_num(_a0) == get_last_irreversible_block_num(_a1)
    assert get_last_irreversible_block_num(_w0) == get_last_irreversible_block_num(_w1)
    assert get_last_irreversible_block_num(_a0) == get_last_irreversible_block_num(_w0)
