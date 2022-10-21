from .local_tools import wait, fork_log, get_last_head_block_number, get_last_irreversible_block_num, wait_for_final_block
import test_tools as tt
import time

def test_obi_throw_exception_00(prepare_obi_throw_exception_00):
    # start - A network (consists of a 'A' network(10 witnesses) + a 'B' network(11 witnesses)) produces blocks

    # A witness from the `A` network has an exception - during `1` block this sub network can't produce.
    # After production resuming, both sub networks link together and LIB increases, because during the exception a block is produced by a witness from `A` network

    sub_networks_data   = prepare_obi_throw_exception_00['sub-networks-data']
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

    blocks_before_exception = 13
    blocks_after_exception  = 5
    blocks_wait             = 1

    tt.logger.info(f'Before an exception')
    wait(blocks_before_exception, logs, witness_node_0)

    _a0 = logs[0].collector
    last_lib_01                                 = get_last_irreversible_block_num(_a0)

    tt.logger.info(f'Artificial exception is thrown during {blocks_wait} block')
    witness_node_0.api.debug_node.debug_throw_exception(throw_exception = True)

    wait(blocks_wait, logs, witness_node_1)

    tt.logger.info(f'Artificial exception is disabled')
    witness_node_0.api.debug_node.debug_throw_exception(throw_exception = False)

    wait(blocks_after_exception, logs, witness_node_0)

    _a0 = logs[0].collector
    _w0 = logs[1].collector
    _a1 = logs[2].collector
    _w1 = logs[3].collector

    assert get_last_head_block_number(_a0) > last_lib_01

    wait_for_final_block(witness_node_0, logs, [_a0, _w0, _a1, _w1])