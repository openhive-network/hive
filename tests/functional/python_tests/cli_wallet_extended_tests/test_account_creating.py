import pytest

import test_tools as tt


def test_account_creation(wallet):
    wallet.api.create_account('initminer', 'alice', '{}')
    assert wallet.api.list_accounts('a', 1) == ['alice']


def test_creation_of_account_with_too_long_name(wallet):
    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        wallet.api.create_account('initminer', 'too-long-account-name', '{}')

    error_message = exception.value.response['error']['message']
    assert 'Account name too-long-account-name is invalid' in error_message
