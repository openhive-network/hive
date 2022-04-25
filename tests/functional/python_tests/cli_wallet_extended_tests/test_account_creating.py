import pytest

from test_tools.exceptions import CommunicationError


def test_account_creation(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    assert wallet.api.list_accounts('a', 1) == ['alice']


def test_creation_of_account_with_too_long_name(wallet):
    with pytest.raises(CommunicationError) as exception:
        wallet.api.create_account('initminer', 'too-long-account-name', '{}')

    error_message = exception.value.response['error']['message']
    assert 'Invalid account name. It is too long, should have up to 16 characters.' in error_message
