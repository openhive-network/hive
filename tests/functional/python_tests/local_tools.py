from datetime import datetime

from typing import List

from pathlib import Path

import os

import test_tools as tt

def parse_datetime(datetime_: str) -> datetime:
    return datetime.strptime(datetime_, '%Y-%m-%dT%H:%M:%S')

def init_network( init_node, all_witness_names : List[str], key : str = None, block_log_directory_name : str = None):

    tt.logger.info('Attaching wallets...')
    wallet = tt.Wallet(attach_to=init_node)
    # We are waiting here for block 43, because witness participation is counting
    # by dividing total produced blocks in last 128 slots by 128. When we were waiting
    # too short, for example 42 blocks, then participation equals 42 / 128 = 32.81%.
    # It is not enough, because 33% is required. 43 blocks guarantee, that this
    # requirement is always fulfilled (43 / 128 = 33.59%, which is greater than 33%).
    tt.logger.info('Wait for block 43 (to fulfill required 33% of witness participation)')
    init_node.wait_for_block_with_number(43)

    # Prepare witnesses on blockchain
    with wallet.in_single_transaction():
        for name in all_witness_names:
            if key is None:
                wallet.api.create_account('initminer', name, '')
            else:
                wallet.api.create_account_with_keys('initminer', name, '', key, key, key, key)
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(1000))
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.update_witness(
                name, "https://" + name,
                tt.Account(name).public_key,
                {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
            )

    tt.logger.info('Wait 21 blocks to schedule newly created witnesses into future slate')
    init_node.wait_number_of_blocks(21)
    tt.logger.info('Wait 21 blocks for future slate to become active slate')
    init_node.wait_number_of_blocks(21)

    active_witnesses = init_node.api.database.get_active_witnesses()["witnesses"]
    tt.logger.info(f"Witness state after voting: {active_witnesses}")
    assert len(active_witnesses) == 21

    # Reason of this wait is to enable moving forward of irreversible block
    tt.logger.info('Wait 21 blocks (when every witness sign at least one block)')
    init_node.wait_number_of_blocks(21)

    result = wallet.api.info()
    head_block_num = result['head_block_number']
    timestamp = init_node.api.block.get_block(block_num=head_block_num)['block']['timestamp']
    tt.logger.info(f'head block timestamp: {timestamp}')

    if block_log_directory_name is not None:
        if os.path.exists(block_log_directory_name):
            Path(block_log_directory_name + '/block_log').unlink(missing_ok=True)
        else:
            os.mkdir(block_log_directory_name)

        init_node.close()
        init_node.get_block_log(include_index=False).copy_to(block_log_directory_name)

        with open(block_log_directory_name + '/timestamp', 'w') as f:
            f.write(f'{timestamp}')

    return wallet
