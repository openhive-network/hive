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
            # Extension is in legacy format
            ['comment_payout_beneficiaries', {'beneficiaries': [{'account': 'bob', 'weight': 100}]}],
        ),
        (
            'block',
            # Extension in HF26 format
            {
                'type': 'comment_payout_beneficiaries',
                'value': {'beneficiaries': [{'account': 'bob', 'weight': 100}]}
            },
        ),
    ]
)
@pytest.mark.testnet
def test_change_comment_operation(node, api, expected_extension):
    wallet = tt.Wallet(attach_to=node)

    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    create_account_and_fund_it(wallet, 'bob', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))

    wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    transaction = {
        'operations': [
            [
                'comment_options',
                {
                    'author': 'alice',
                    'permlink': 'permlink',
                    'max_accepted_payout': '10.000 TBD',
                    'percent_hbd': 10,
                    'allow_votes': False,
                    'allow_curation_rewards': False,
                    'extensions': [
                        [
                            'comment_payout_beneficiaries',
                            {'beneficiaries': [{'account': 'bob', 'weight': 100}]}
                        ]
                    ]
                }
            ]
        ],
        'extensions': [],
        'signatures': [],
    }

    transaction = wallet.api.sign_transaction(transaction)
    block_num = transaction['block_num']

    extension = get_extension_from_operation(node, api, block_num)
    assert extension == expected_extension


@pytest.mark.parametrize(
    'api, expected_extension', [
        ('block', {'type': 'update_proposal_end_date', 'value': {'end_date': PROPOSAL_END_DATE_AFTER_UPDATE}}),
        ('condenser', ['update_proposal_end_date', {'end_date': PROPOSAL_END_DATE_AFTER_UPDATE}]),
    ]
)
@pytest.mark.testnet
def test_update_proposal_operation(node, api, expected_extension):
    wallet = tt.Wallet(attach_to=node)

    create_account_and_fund_it(wallet, 'alice', tbds=tt.Asset.Tbd(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'permlink', '', 'parent-permlink', 'title', 'body', '{}')
    wallet.api.create_proposal('alice', 'alice', PROPOSAL_START_DATE, PROPOSAL_END_DATE, tt.Asset.Tbd(100), 'subject-1', 'permlink')
    operation = wallet.api.update_proposal(0, 'alice', tt.Asset.Tbd(10), 'subject-1', 'permlink', PROPOSAL_END_DATE_AFTER_UPDATE)
    block_num = operation['block_num']
    extension = get_extension_from_operation(node, api, block_num)

    assert extension == expected_extension


def get_extension_from_operation(node, api: str, block_num: int):
    if api == 'block':
        block = getattr(node.api, api).get_block(block_num=block_num)
        operation = block['block']['transactions'][0]['operations'][0]
        return operation['value']['extensions'][0]

    if api == 'condenser':
        block = getattr(node.api, api).get_block(block_num)
        operation = block['transactions'][0]['operations'][0][1]
        return operation['extensions'][0]

    raise RuntimeError("Shouldn't be ever reached")
