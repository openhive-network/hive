from pathlib import Path
import pytest

from concurrent.futures import ThreadPoolExecutor

import test_tools as tt

import shared_tools.complex_networks_helper_functions as sh

memo_cnt            = 0

break_cnt           = 0
break_limit         = 250

def generate_break(wallet: tt.Wallet, node: tt.ApiNode, identifier: int):
    global break_cnt
    global break_limit

    while break_cnt < break_limit:
        sh.info("m4", wallet)
        node.wait_number_of_blocks(1)
        break_cnt += 1
    return f'[break {identifier}] Breaking activated...'

def trx_creator(wallet: tt.Wallet, identifier: int):
    global memo_cnt

    global break_cnt
    global break_limit

    while break_cnt < break_limit:
        wallet.api.transfer_nonblocking('initminer', 'null', tt.Asset.Test(1), str(memo_cnt))
        memo_cnt += 1
    return f'[break {identifier}] Creating transactions finished...'

def test_many_forks_node_with_time_offset(prepare_4_4_4_4_4):
    global break_cnt
    global break_limit

    tt.logger.info(f'Start test_many_forks_node_with_time_offset')

    networks_builder = prepare_4_4_4_4_4

    node_under_test = networks_builder.networks[1].node('ApiNode0')
    beta_wallet = tt.Wallet(attach_to = node_under_test)

    _, break_cnt = sh.info('m4', beta_wallet)
    tt.logger.info(f'initial break_cnt: {break_cnt}')

    _futures                = []
    _push_threads           = 2
    _generate_break_threads = 1
    with ThreadPoolExecutor(max_workers = _push_threads + _generate_break_threads) as executor:
        for i in range(_push_threads):
            _futures.append(executor.submit(trx_creator, beta_wallet, i))

        _futures.append(executor.submit(generate_break, beta_wallet, node_under_test, 0))

    tt.logger.info("results:")
    for future in _futures:
        tt.logger.info(f'{future.result()}')

