from test_tools import Asset
from test_tools.communication import request


def test_transfer_exercise(node, wallet):
    account_first = wallet.api.create_account('initminer', 'joe', '{}')
    account_second = wallet.api.create_account('initminer', 'alison', '{}')

    wallet.api.delegate_rc('initminer', ['joe'], 10)
    x = wallet.api.transfer('initminer', 'joe', Asset.Test(1000), 'memo_first')

    wallet.api.transfer('joe', 'alison', Asset.Test(400), 'memo_second')

    alison_ballance = wallet.api.get_account('alison')

    assert alison_ballance['balance'] == Asset.Test(400)


