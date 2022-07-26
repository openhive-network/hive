import pytest

import test_tools as tt

from ....local_tools import create_account_and_fund_it


@pytest.mark.parametrize(
    'api, expected_extension', [
        (
            'condenser',
            ['comment_payout_beneficiaries', {'beneficiaries': [{'account': 'bob', 'weight': 100}]}],  # Extension is in legacy format
        ),
        (
            'block',
            {
                'type': 'comment_payout_beneficiaries',
                'value': {
                    'beneficiaries': [
                        {
                            'account': 'bob',
                            'weight': 100
                        }]
                }
            },  # Extension in HF26 format
        ),
    ]
)
def test_extension_serialization_format(node, api, expected_extension):
    wallet = tt.Wallet(attach_to=node)

    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    change_comment_operation = wallet.api.change_comment_options(
        'alice', 'permlink', tt.Asset.Tbd(10), 10, False, False,
        beneficiaries={'beneficiaries': [{'account': 'bob', 'weight': 100}]},
    )

    block = node.api.condenser.get_block(change_comment_operation['block_num'])
    operations = block['transactions'][0]['operations']
    extension = operations[0][1]['extensions'][0]

    assert extension == expected_extension
