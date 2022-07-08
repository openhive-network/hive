import json
from pathlib import Path

import test_tools as tt

from local_tools import create_account_and_fund_it


def prepare_blocklog():
    node = tt.InitNode()
    node.run()

    wallet = tt.Wallet(attach_to=node, additional_arguments=['--transaction-serialization=legacy'])

    alice = create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(1000), vests=tt.Asset.Test(1000000),
                                       tbds=tt.Asset.Tbd(1000))
    bob = create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(1000), vests=tt.Asset.Test(1000000),
                                     tbds=tt.Asset.Tbd(1000))

    # change_recovery_account preparation
    carol = create_account_and_fund_it(wallet, 'carol', 'alice', tt.Asset.Test(10), tt.Asset.Test(100),
                                       tt.Asset.Tbd(10))
    pub_keys = []
    pub_keys.append(alice['operations'][0][1]['active']['key_auths'][0][0])
    pub_keys.append(bob['operations'][0][1]['active']['key_auths'][0][0])
    pub_keys.append(carol['operations'][0][1]['active']['key_auths'][0][0])

    priv_keys = []
    for key in pub_keys:
        priv_keys.append(wallet.api.get_private_key(key))

    save_keys_to_json_file(priv_keys)

    ####################################################################################################################
    # Cancel_transfer_from_savings preparation
    wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(10), 'memo')
    wallet.api.transfer_from_savings('alice', 1, 'bob', tt.Asset.Test(1), 'memo')

    # Cancel_order preparation
    wallet.api.create_order('alice', 1, tt.Asset.Test(1), tt.Asset.Tbd(1), False, 1000)

    # Create_proposal preparation
    wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    ####################################################################################################################

    node.wait_number_of_blocks(21)
    node.close()
    node.get_block_log(include_index=True).copy_to(Path(__file__).parent.absolute())


def save_keys_to_json_file(keys):
    dictionary = {}
    for key_num in range(len(keys)):
        dictionary[key_num] = keys[key_num]

    with open(Path().absolute() / 'private_keys.json', 'w') as file:
        json.dump(dictionary, file)


if __name__ == "__main__":
    prepare_blocklog()
