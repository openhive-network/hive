import test_tools as tt
from ....local_tools import date_from_now


def test_escrow_transfer(wallet):
    with wallet.in_single_transaction():
        wallet.api.create_account('initminer', 'alice', '{}')
        wallet.api.create_account('initminer', 'bob', '{}')

    wallet.api.escrow_transfer('initminer', 'alice', 'bob', 99, tt.Asset.Tbd(10), tt.Asset.Test(10),
                                        tt.Asset.Tbd(10), date_from_now(weeks=16), date_from_now(weeks=20), '{}')
