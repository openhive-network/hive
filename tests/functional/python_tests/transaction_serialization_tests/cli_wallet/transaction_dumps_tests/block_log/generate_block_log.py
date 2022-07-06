import json
import pytest

from pathlib import Path

import test_tools as tt


@pytest.fixture(params=['legacy', 'hf26'])
def wallet(node, request):
    type_of_serialization = request.param
    wallet = tt.Wallet(attach_to=node, additional_arguments=[f'--store-transaction={request.fspath.purebasename}',
                                                             f'--transaction-serialization={type_of_serialization}'])

    yield wallet, type_of_serialization

    node.wait_number_of_blocks(21)

    save_block_log_length_to_json_file(request.param, node.get_last_block_number())

    node.close()

    node.get_block_log(include_index=True).copy_to(Path(__file__).parent.absolute() / type_of_serialization)


def test_prepare_blocklog(node, wallet):
    # Account creation, powering and export private keys
    type_of_serialization = wallet[1]
    wallet = wallet[0]
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

    if type_of_serialization == 'legacy':
        alice_pub_key = alice['operations'][0][1]['active']['key_auths'][0][0]
        bob_pub_key = bob['operations'][0][1]['active']['key_auths'][0][0]

    elif type_of_serialization == 'hf26':
        alice_pub_key = alice['operations'][0]['value']['active']['key_auths'][0][0]
        bob_pub_key = bob['operations'][0]['value']['active']['key_auths'][0][0]

    alice_priv_key = wallet.api.get_private_key(alice_pub_key)
    bob_priv_key = wallet.api.get_private_key(bob_pub_key)

    save_keys_to_json_file(type_of_serialization, alice_priv_key, bob_priv_key)
    ####################################################################################################################

    wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(10), 'memo')
    wallet.api.transfer_from_savings('alice', 1, 'bob', tt.Asset.Test(1), 'memo')

    wallet.api.create_order('alice', 1, tt.Asset.Test(1), tt.Asset.Tbd(1), False, 1000)


def save_keys_to_json_file(folder_name, *keys):
    dict = {}
    for key_num in range(len(keys)):
        dict[key_num] = keys[key_num]

    with open(Path().absolute() / folder_name / 'private_keys.json', 'w') as file:
        json.dump(dict, file)


def save_block_log_length_to_json_file(folder_name, block_log_length):
    dict = {'block_log_length': block_log_length}

    with open(Path().absolute() / folder_name / 'block_log_length.json', 'w') as file:
        json.dump(dict, file)


# if __name__ == '__main__':
#     prepare_blocklog()
