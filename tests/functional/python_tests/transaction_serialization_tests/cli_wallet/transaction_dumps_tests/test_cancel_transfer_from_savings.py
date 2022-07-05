import pytest

import test_tools as tt

from ......local_tools import create_account_and_fund_it

@pytest.mark.testnet
def test_cancel_transfer_from_savings(nai_wallet):
    # create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(10))
    # wallet.api.create_account('initminer', 'bob', '{}')
    #
    # wallet.api.transfer_to_savings('initminer', 'alice', tt.Asset.Test(10), 'memo')
    #
    # wallet.api.transfer_from_savings('alice', 1, 'bob', tt.Asset.Test(1), 'memo')
    nai_wallet.api.import_key('5Ki4zhn47Gf5gDR5xgthWdnJw4DgX5bKQSScG1einbCWDmJkpWd') #alice
    nai_wallet.api.import_key('5KFSE53CYbRuGr3EjrajSyDHCV6L277DPEMLsU1zjxtUvDwKJJ5') #bob
    nai_wallet.api.import_key('5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n') #initminer

    nai_wallet.api.cancel_transfer_from_savings('alice', 1)
