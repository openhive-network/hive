import concurrent.futures
from copy import deepcopy
import math
from pathlib import Path
from typing import List

import test_tools as tt


def prepare_node_with_proposal_votes():
    node = tt.InitNode()
    node.run()
    wallet = tt.Wallet(attach_to=node)

    amount_of_all_accounts = 20_000
    accounts = get_account_names(wallet.create_accounts(amount_of_all_accounts))

    wallet.create_account("alice", hives=tt.Asset.Test(100), vests=tt.Asset.Test(100), hbds=tt.Asset.Tbd(100))
    wallet.api.post_comment("alice", "permlink", "", "parent-permlink", "title", "body", "{}")
    wallet.api.create_proposal(
        "alice",
        "alice",
        tt.Time.now(),
        tt.Time.from_now(days=10),
        tt.Asset.Tbd(1 * 100),
        "subject",
        "permlink",
    )

    transaction_template = {
        'ref_block_num': 0,
        'ref_block_prefix': 0,
        'expiration': '2022-11-23T11:55:36',
        'operations': [],
        'extensions': [],
        'signatures': [],
        'transaction_id': '0000000000000000000000000000000000000000',
        'block_num': 0,
        'transaction_num': 0
    }

    def generate_operations_for_receiver(receiver: str, amount=tt.Asset.Test(0.1)) -> list:
        return [
            ['transfer_to_vesting', {'from': 'initminer', 'to': receiver, 'amount': str(amount)}],
            ['update_proposal_votes', {'voter': receiver, 'proposal_ids': [0], 'approve': True, 'extensions': []}]
        ]

    def generate_and_broadcast(acc_num_start, acc_num_stop):
        accs = list(accounts[acc_num_start:acc_num_stop])
        transaction = deepcopy(transaction_template)
        for name in accs:
            transaction['operations'].extend(generate_operations_for_receiver(name))
        try:
            wallet.api.sign_transaction(transaction)
        except Exception as e:
            tt.logger.error(e)
            raise e
        else:
            tt.logger.info(f'Finished: {accs[0]}')
        return accs[0]

    account_per_chunk = 750
    max_threads = 6
    futures = []
    with concurrent.futures.ThreadPoolExecutor(
            max_workers=min(math.ceil(amount_of_all_accounts / account_per_chunk), max_threads)
    ) as executor:
        for n in range(0, len(accounts), account_per_chunk):
            futures.append(executor.submit(generate_and_broadcast, n, n + account_per_chunk))
            tt.logger.info(f"Pack generated: {n}")

        for future in futures:
            tt.logger.info(f'Got: {future.result()}')

    node.wait_number_of_blocks(21)

    result = wallet.api.info()
    head_block_num = result['head_block_number']
    timestamp = node.api.block.get_block(block_num=head_block_num)['block']['timestamp']
    tt.logger.info(f'head block timestamp: {timestamp}')

    with open('timestamp', 'w') as f:
        f.write(f'{timestamp}')

    node.close()
    node.block_log.copy_to(Path(__file__).parent.absolute())


def get_account_names(accounts: List[tt.Account]) -> List[str]:
    return [account.name for account in accounts]


if __name__ == '__main__':
    prepare_node_with_proposal_votes()
