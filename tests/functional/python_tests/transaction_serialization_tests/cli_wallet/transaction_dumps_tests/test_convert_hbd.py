import pytest

import test_tools as tt

from ......local_tools import create_account_and_fund_it


@pytest.mark.testnet
def test_convert_hbd(node, wallet):
    create_account_and_fund_it(wallet, 'alice', vests=tt.Asset.Test(100), tbds=tt.Asset.Tbd(100))
    wallet.api.convert_hbd('alice', tt.Asset.Tbd(1))
