import json

from pathlib import Path

import test_tools as tt

WITNESSES_NAMES = [f'witness-{i}' for i in range(20)]  # 21-st is initminer

ACCOUNTS = [f'account-{i}' for i in range(10)]


def test_prepare_blocklog(node, wallet):
    # Account creation, powering and export private keys

    alice = wallet.api.create_account('initminer', 'alice', '{}')
    with wallet.in_single_transaction():
        wallet.api.transfer('initminer', 'alice', tt.Asset.Test(1000), 'memo')
        wallet.api.transfer_to_vesting('initminer', 'alice', tt.Asset.Test(1000000))
        wallet.api.transfer('initminer', 'alice', tt.Asset.Tbd(1000), 'memo')

    bob = wallet.api.create_account('initminer', 'bob', '{}')
    with wallet.in_single_transaction():
        wallet.api.transfer('initminer', 'bob', tt.Asset.Test(1000), 'memo')
        wallet.api.transfer_to_vesting('initminer', 'bob', tt.Asset.Test(1000000))
        wallet.api.transfer('initminer', 'bob', tt.Asset.Tbd(1000), 'memo')

    alice_pub_key = alice['operations'][0]['value']['active']['key_auths'][0][0]
    bob_pub_key = bob['operations'][0]['value']['active']['key_auths'][0][0]

    alice_priv_key = wallet.api.get_private_key(alice_pub_key)
    bob_priv_key = wallet.api.get_private_key(bob_pub_key)

    save_keys_to_json_file(alice_priv_key, bob_priv_key)
    ####################################################################################################################

    wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(10), 'memo')
    wallet.api.transfer_from_savings('alice', 1, 'bob', tt.Asset.Test(1), 'memo')
    node.wait_number_of_blocks(21)

    node.close()

    node.get_block_log(include_index=True).copy_to(Path(__file__).parent.absolute())



def save_keys_to_json_file(*keys):
    dict = {}
    for key_num in range(len(keys)):
        dict[key_num] = keys[key_num]

    path = Path().absolute()
    with open(Path().absolute() / 'private_keys.json', 'w') as f:
        json.dump(dict, f)
    print()

def test_prepare_block_log_with_witnesses(node, wallet):
    # node, wallet = prepare_node_with_witnesses(WITNESSES_NAMES)

    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(1000000))
    wallet.api.create_account('initminer', 'bob', '{}')

    wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(1000), 'memo')

    wallet.api.transfer_from_savings('alice', 1, 'bob', tt.Asset.Test(100), 'memo')

    alice = wallet.api.get_account('alice')
    bob = wallet.api.get_account('bob')
    initminer = wallet.api.get_account('initminer')

    alice_pub_key = alice['active']['key_auths'][0][0]
    bob_pub_key = bob['active']['key_auths'][0][0]
    initminer_pub_key = initminer['active']['key_auths'][0][0]

    alice_priv_key = wallet.api.get_private_key(alice_pub_key)
    bob_priv_key = wallet.api.get_private_key(bob_pub_key)
    initminer_priv_key = wallet.api.get_private_key(initminer_pub_key)

    node.wait_number_of_blocks(21)

    node.close()

    node.get_block_log(include_index=True).copy_to(Path(__file__).parent.absolute())
    pass


# def prepare_node_with_witnesses(witnesses_names):
#     node = tt.InitNode()
#     for name in witnesses_names:
#         witness = tt.Account(name)
#         node.config.witness.append(witness.name)
#         node.config.private_key.append(witness.private_key)
#
#     node.run()
#     wallet = tt.Wallet(attach_to=node)
#
#     with wallet.in_single_transaction():
#         for name in witnesses_names:
#             wallet.api.create_account('initminer', name, '')
#
#     with wallet.in_single_transaction():
#         for name in witnesses_names:
#             wallet.api.transfer_to_vesting("initminer", name, tt.Asset.Test(1000))
#
#     with wallet.in_single_transaction():
#         for name in witnesses_names:
#             wallet.api.update_witness(
#                 name, "https://" + name,
#                 tt.Account(name).public_key,
#                 {"account_creation_fee": tt.Asset.Test(3), "maximum_block_size": 65536, "sbd_interest_rate": 0}
#             )
#
#     tt.logger.info('Waiting for next witness schedule...')
#     node.wait_for_block_with_number(22)
#
#     return node, wallet


if __name__ == '__main__':
    prepare_block_log_with_witnesses()
