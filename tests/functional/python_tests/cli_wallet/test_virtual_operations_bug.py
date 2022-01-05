import time
from datetime import datetime, timezone, timedelta
import re

import pytest

from test_tools import Account, Asset, exceptions, logger, Wallet, World
from test_tools.exceptions import CommunicationError

def test_account_history(wallet:Wallet, world:World):
    node = world.create_init_node('InitNode1')
    node.run()
    wallet = Wallet(attach_to=node)
    z= node.get_http_endpoint()
    logger.info(z)

    wallet.api.create_account('initminer', 'bob', '{}')
    wallet.api.transfer_to_vesting('initminer', 'bob', Asset.Test(20000000))
    wallet.api.post_comment('bob', 'hello-world', '', 'xyz', 'something about world', 'just nothing', '{}')
    wallet.api.create_account('initminer', 'alice', '{}')
    node.wait_for_block_with_number(10)
    with pytest.raises(exceptions.CommunicationError):
        wallet.api.vote('alice', 'bob', 'hello-world', 97)

    node.wait_for_block_with_number(31)
    api1 = node.api.account_history.get_account_history(
        account='initminer',
        start=-1,
        limit=1000
        )

#sprawdź działania buga z "account_history_api.enum_virtual_ops"

    time.sleep(23213)

