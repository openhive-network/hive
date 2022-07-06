import pytest

import test_tools as tt

from .local_tools import import_private_keys_from_json_file
from ......local_tools import create_account_and_fund_it


@pytest.mark.test
def test_cancel_order(wallet):
    # create_account_and_fund_it(wallet, 'alice', tests=tt.Asset.Test(100), vests=tt.Asset.Test(100))
    # wallet.api.create_order('alice', 1, tt.Asset.Test(1), tt.Asset.Tbd(1), False, 1000)
    # node.wait_number_of_blocks(21)
    # wallet.close()
    # node.close()
    # node.run(time_offset='+5s')
    # wallet.run(timeout=10)
    type_of_serialization = wallet[1]
    wallet = wallet[0]
    import_private_keys_from_json_file(wallet, type_of_serialization)
    wallet.api.cancel_order('alice', 1, broadcast=False)
    # node.run(time_offset='@2022-07-08 08:00:00')
    