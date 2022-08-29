from .local_tools import wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num
import test_tools as tt
import time

def test_obi_throw_exception_02(prepare_obi_throw_exception_02):
    # start - A network (consists of a 'A' network(10 witnesses) + a 'B' network(11 witnesses)) produces blocks

    # A witnesses from both networks have an exception in the same time - during 20 seconds these sub networks can't produce
    # After production resuming, both sub networks link together and LIB increases,
    # because sending transaction with `witness_block_approve_operation` inf witness plugin is sent when `finish_apply_block` event occurs

    sub_networks_data   = prepare_obi_throw_exception_02['sub-networks-data']
    sub_networks        = sub_networks_data[0]
    assert len(sub_networks) == 2

    api_node_0          = sub_networks[0].node('ApiNode0')
    witness_node_0      = sub_networks[0].node('WitnessNode0')

    api_node_1          = sub_networks[1].node('ApiNode1')
    witness_node_1      = sub_networks[1].node('WitnessNode1')

    logs = []

    logs.append(fork_log("a0", tt.Wallet(attach_to = api_node_0)))
    logs.append(fork_log("w0", tt.Wallet(attach_to = witness_node_0)))

    logs.append(fork_log("a1", tt.Wallet(attach_to = api_node_1)))
    logs.append(fork_log("w1", tt.Wallet(attach_to = witness_node_1)))

    blocks                  = 10
    blocks_after_exception  = 20

    tt.logger.info(f'Before an exception')
    wait(blocks, logs, witness_node_0)

    _a0 = logs[0].collector
    last_lib_01                                 = get_last_irreversible_block_num(_a0)

    tt.logger.info(f'Artificial exception is thrown during 30 seconds')
    witness_node_0.api.debug_node.debug_throw_exception(throw_exception = True)
    witness_node_1.api.debug_node.debug_throw_exception(throw_exception = True)

    time.sleep(30)

    tt.logger.info(f'Artificial exception is disabled')
    witness_node_0.api.debug_node.debug_throw_exception(throw_exception = False)
    witness_node_1.api.debug_node.debug_throw_exception(throw_exception = False)

    wait(blocks_after_exception, logs, witness_node_0)

    _a0 = logs[0].collector
    _w0 = logs[1].collector
    _a1 = logs[2].collector
    _w1 = logs[3].collector

    assert get_last_head_block_number(_a0) > last_lib_01 + 1

    assert get_last_head_block_number(_a0) == get_last_head_block_number(_a1)
    assert get_last_head_block_number(_w0) == get_last_head_block_number(_w1)
    assert get_last_head_block_number(_a0) == get_last_head_block_number(_w0)

    assert get_last_irreversible_block_num(_a0) == get_last_irreversible_block_num(_a1)
    assert get_last_irreversible_block_num(_w0) == get_last_irreversible_block_num(_w1)
    assert get_last_irreversible_block_num(_a0) == get_last_irreversible_block_num(_w0)
