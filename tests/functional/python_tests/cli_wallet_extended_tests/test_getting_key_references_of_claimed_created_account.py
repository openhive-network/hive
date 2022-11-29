# This is a regression test for problem described in following issue:
# https://gitlab.syncad.com/hive/hive/-/issues/113

import test_tools as tt


def test_getting_key_references_of_claimed_created_account(node, wallet):
    tt.logger.info('Waiting until initminer will be able to create account...')
    node.api.debug_node.debug_generate_blocks(debug_key=tt.Account('initminer').private_key, count=30, skip=0,
                                              miss_blocks=0, edit_if_needed=True)

    account = tt.Account('alice')
    key = account.public_key
    wallet.api.claim_account_creation('initminer', tt.Asset.Test(0))
    wallet.api.create_funded_account_with_keys(
        'initminer', account.name, tt.Asset.Test(0), 'memo', '{}', key, key, key, key
    )

    assert node.api.condenser.get_key_references([account.public_key]) == [[account.name]]
