import test_tools as tt
from ....local_tools import create_account_and_fund_it, date_from_now


def test_escrow_release(wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(50))
    create_account_and_fund_it(wallet, 'bob', vests=tt.Asset.Test(50))

    wallet.api.escrow_transfer('initminer', 'alice', 'bob', 99, tt.Asset.Tbd(10), tt.Asset.Test(10),
                               tt.Asset.Tbd(10), date_from_now(weeks=16), date_from_now(weeks=20), '{}')

    wallet.api.escrow_approve('initminer', 'alice', 'bob', 'bob', 99, True)

    wallet.api.escrow_approve('initminer', 'alice', 'bob', 'alice', 99, True)

    wallet.api.escrow_dispute('initminer', 'alice', 'bob', 'initminer', 99)

    wallet.api.escrow_release('initminer', 'alice', 'bob', 'bob', 'alice', 99, tt.Asset.Tbd(10), tt.Asset.Test(10))
