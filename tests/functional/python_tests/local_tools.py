from datetime import datetime

from typing import List

import test_tools as tt

def parse_datetime(datetime_: str) -> datetime:
    return datetime.strptime(datetime_, '%Y-%m-%dT%H:%M:%S')

def prepare_witnesses( init_node, all_witness_names : List[str] ):

    tt.logger.info('Attaching wallets...')
    wallet = tt.Wallet(attach_to=init_node)
    # We are waiting here for block 43, because witness participation is counting
    # by dividing total produced blocks in last 128 slots by 128. When we were waiting
    # too short, for example 42 blocks, then participation equals 42 / 128 = 32.81%.
    # It is not enough, because 33% is required. 43 blocks guarantee, that this
    # requirement is always fulfilled (43 / 128 = 33.59%, which is greater than 33%).
    tt.logger.info('Wait for block 43 (to fulfill required 33% of witness participation)')
    init_node.wait_for_block_with_number(43)

    witness_details = []
    # Prepare witnesses on blockchain
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.create_account('initminer', name, '')
    with wallet.in_single_transaction():
        for name in all_witness_names:
            wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(1000))
    with wallet.in_single_transaction():
        for name in all_witness_names:
            key = tt.Account(name).public_key
            witness_details.append( (name, key) )

            wallet.api.update_witness(
                name, "https://" + name,
                key,
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

    # Network should be set up at this time, with 21 active witnesses, enough participation rate
    # and irreversible block number lagging behind around 15-20 blocks head block number
    result = wallet.api.info()
    irreversible = result["last_irreversible_block_num"]
    head = result["head_block_num"]
    tt.logger.info(f'Network prepared, irreversible block: {irreversible}, head block: {head}')

    # with fast confirm, irreversible will usually be = head
    # assert irreversible + 10 < head
    return witness_details

def enable_witnesses(wallet : tt.Wallet, witness_details : list):
    for witness_detail in witness_details:
        name = witness_detail[0]
        key = witness_detail[1]
        wallet.api.update_witness(
            name, "https://" + name,
            key,
            {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
        )

def disable_witnesses(wallet : tt.Wallet, witness_details : list):
    key = 'TST5NUU7M7pmqMpMHUwscgUBMuwLQE56MYwCLF7q9ZGB6po1DMNoG'
    for witness_detail in witness_details:
        name = witness_detail[0]
        wallet.api.update_witness(
            name, "https://" + name,
            key,
            {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
        )
