import pytest

import test_tools as tt

from ......local_tools import create_account_and_fund_it, date_from_now


@pytest.mark.testnet
def test_remove_proposal(wallet):
    proposal_creator_name = 'alice'

    create_account_and_fund_it(wallet, proposal_creator_name, tbds=tt.Asset.Tbd(1000000), vests=tt.Asset.Test(1000000))

    wallet.api.post_comment(proposal_creator_name, 'permlink', '', 'parent-permlink', 'title', 'body', '{}')

    wallet.api.create_proposal(proposal_creator_name, proposal_creator_name, date_from_now(weeks=16),
                               date_from_now(weeks=20), tt.Asset.Tbd(1000), 'subject-1', 'permlink')

    account_id = wallet.api.get_account(proposal_creator_name)['id']

    wallet.api.remove_proposal('initminer', [account_id])
