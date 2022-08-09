from .local_tools import wait, fork_log
import test_tools as tt

def test_obi_throw_exception_01(prepared_sub_networks_10_11):
    sub_networks_data   = prepared_sub_networks_10_11['sub-networks-data']
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

    blocks = 10

    tt.logger.info(f'Before an exception')
    wait(blocks, logs, witness_node_0)

    tt.logger.info(f'Artificial exception is thrown during {blocks} blocks generation')
    witness_node_0.api.debug_node.debug_throw_exception(throw_exception = True)
    witness_node_1.api.debug_node.debug_throw_exception(throw_exception = True)

    wait(1, logs, witness_node_0)

    tt.logger.info(f'Artificial exception is disabled')
    witness_node_0.api.debug_node.debug_throw_exception(throw_exception = False)
    witness_node_1.api.debug_node.debug_throw_exception(throw_exception = False)

    wait(blocks, logs, witness_node_0)
