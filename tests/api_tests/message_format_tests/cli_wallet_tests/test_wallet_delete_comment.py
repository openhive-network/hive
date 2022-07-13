import pytest

import test_tools as tt

from ....local_tools import create_account_and_fund_it


@pytest.mark.parametrize(
    'account_value, permlink_value',
    [
        ('alice', 'fake-permlink'),
        ('nonexistent_acc', 'test-permlink'),
    ]
)
def test_delete_comment_with_incorrect_value(node, wallet, account_value, permlink_value):
    create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    wallet.api.post_comment('alice', 'test-permlink', '', 'someone', 'test-title', 'this is a body', '{}')

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.delete_comment(account_value, permlink_value)


@pytest.mark.parametrize(
    'account_value, permlink_value', [
        # wrong account values
        (-1, 'example-permlink'),
        (True, 'example-permlink'),
        ([], 'example-permlink'),

        # wrong permlink values
        ('initminer', -1),
        ('initminer', True),
        ('initminer', []),
    ]
)
def test_delete_comment_with_incorrect_type_of_argument(node, wallet, account_value, permlink_value):
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.delete_comment(account_value, permlink_value)
