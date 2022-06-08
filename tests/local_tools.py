from datetime import datetime, timedelta
from typing import Optional

from test_tools import Asset


def create_account_and_fund_it(wallet, name: str, tests: Optional[Asset.Test] = None,
                               vests: Optional[Asset.Test] = None, tbds: Optional[Asset.Tbd] = None):
    assert any(asset is not None for asset in [tests, vests, tbds]), 'You forgot to fund account'

    wallet.api.create_account('initminer', name, '{}')

    with wallet.in_single_transaction():
        if tests is not None:
            wallet.api.transfer('initminer', name, tests, 'memo')

        if vests is not None:
            wallet.api.transfer_to_vesting('initminer', name, vests)

        if tbds is not None:
            wallet.api.transfer('initminer', name, tbds, 'memo')


def date_from_now(*, weeks):
    future_data = datetime.now() + timedelta(weeks=weeks)
    return future_data.strftime('%Y-%m-%dT%H:%M:%S')
