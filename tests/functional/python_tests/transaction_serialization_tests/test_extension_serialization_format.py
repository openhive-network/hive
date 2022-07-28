import pytest

import test_tools as tt

from ....local_tools import create_account_and_fund_it, date_from_now

PROPOSAL_START_DATE = date_from_now(weeks=16)
PROPOSAL_END_DATE = date_from_now(weeks=20)
PROPOSAL_END_DATE_AFTER_UPDATE = date_from_now(weeks=19)


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
def test_change_comment_operation(node, api, expected_extension):
    wallet = tt.Wallet(attach_to=node)

    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    change_comment_operation = wallet.api.change_comment_options(
        'alice', 'permlink', tt.Asset.Tbd(10), 10, False, False,
        beneficiaries={'beneficiaries': [{'account': 'bob', 'weight': 100}]},
    )
    block_num = change_comment_operation['block_num']
    extension = get_extension_from_operation(node, api, block_num)

    assert False


@pytest.mark.parametrize(
    'api, expected_extension', [
        ('block', {'type': 'update_proposal_end_date', 'value': {'end_date': PROPOSAL_END_DATE_AFTER_UPDATE}}),
        ('condenser', ['update_proposal_end_date', {'end_date': PROPOSAL_END_DATE_AFTER_UPDATE}]),
    ]
)
def test_update_proposal_operation(node, api, expected_extension):
    wallet = tt.Wallet(attach_to=node)

    create_account_and_fund_it(wallet, 'alice', tbds=tt.Asset.Tbd(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')
    wallet.api.create_proposal('alice', 'alice', PROPOSAL_START_DATE, PROPOSAL_END_DATE, tt.Asset.Tbd(100), 'subject-1', 'permlink')
    operation = wallet.api.update_proposal(0, 'alice', tt.Asset.Tbd(10), 'subject-1', 'permlink', PROPOSAL_END_DATE_AFTER_UPDATE)
    block_num = operation['block_num']
    extension = get_extension_from_operation(node, api, block_num)

    assert False


def get_extension_from_operation(node, api, block_num):
    if api == 'block':
        block = getattr(node.api, api).get_block(block_num=block_num)
        operation = block['block']['transactions'][0]['operations'][0]
        extension = operation['value']['extensions'][0]
    elif api == 'condenser':
        block = getattr(node.api, api).get_block(block_num)
        operation = block['transactions'][0]['operations'][0][1]
        extension = operation['extensions'][0]
    else:
        raise RuntimeError("Shouldn't be ever reached")

    return extension
