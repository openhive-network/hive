import pytest

import test_tools as tt

from ....local_tools import create_account_and_fund_it


def test_update_account2(node, wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(50))
    new_json_metadata = '{"tags":["example tag"]}'
    new_posting_json_metadata = '{"profile": "Short profile description"}'
    wallet.api.update_account2('alice', new_json_metadata, new_posting_json_metadata,
                               tt.Account('alice').public_key, tt.Account('alice').public_key,
                               tt.Account('alice').public_key, tt.Account('alice').public_key)
    account = wallet.api.get_account('alice')

    assert account['json_metadata'] == new_json_metadata
    assert account['posting_json_metadata'] == new_posting_json_metadata

    for key in ['owner', 'active', 'posting']:
        assert account[key]['key_auths'][0][0] == tt.Account('alice').public_key

    assert account['memo_key'] == tt.Account('alice').public_key


def test_update_nonexistent_account(node, wallet):
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.update_account2('non-existent', '{}', '{}', tt.Account('non-existent').public_key,
                                   tt.Account('non-existent').public_key, tt.Account('non-existent').public_key,
                                   tt.Account('non-existent').public_key)


@pytest.mark.parametrize(
    'json_meta, posting_json_meta',
    [
        ('{"something"="wrong"}', '{}'),
        ('{}', '{"something"="wrong"}')
    ]
)
def test_update_account2_with_incorrect_json(node, wallet, json_meta, posting_json_meta):
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.update_account2('initminer', json_meta, posting_json_meta,
                                   tt.Account('alice').public_key, tt.Account('alice').public_key,
                                   tt.Account('alice').public_key, tt.Account('alice').public_key)


@pytest.mark.parametrize(
    'active, owner, memo, posting',
    [
        ('wrong_active_key', tt.Account('alice').public_key, tt.Account('alice').public_key, tt.Account('alice').public_key),
        (tt.Account('alice').public_key, 'wrong_owner_key', tt.Account('alice').public_key, tt.Account('alice').public_key),
        (tt.Account('alice').public_key, tt.Account('alice').public_key, 'wrong_memo_key', tt.Account('alice').public_key),
        (tt.Account('alice').public_key, tt.Account('alice').public_key, tt.Account('alice').public_key, 'wrong_posting_key'),
    ]
)
def test_update_account2_with_incorrect_keys(node, wallet, active, owner, memo, posting):
    with pytest.raises(tt.exceptions.CommunicationError):
        wallet.api.update_account2('initminer', '{}', '{}', owner, active, posting, memo)
