from test_tools import Account, logger, World, Asset
from .utilities import create_accounts

def test_transfer(node, wallet):
    create_accounts( wallet, 'initminer', ['newaccount', 'newaccount2'] )

    wallet.api.transfer_to_vesting('initminer', 'newaccount', Asset.Test(100))

    _result = wallet.api.get_account('newaccount')
    assert _result['balance'] == Asset.Test(0)
    assert _result['hbd_balance'] == Asset.Tbd(0)
    assert _result['savings_balance'] == Asset.Test(0)
    assert _result['savings_hbd_balance'] == Asset.Tbd(0)
    assert _result['vesting_shares'] != Asset.Vest(0)

    wallet.api.transfer('initminer', 'newaccount', Asset.Test(5.432), 'banana')

    wallet.api.transfer('initminer', 'newaccount', Asset.Tbd(9.169), 'banana2')

    wallet.api.transfer_to_savings('initminer', 'newaccount', Asset.Test(15.432), 'pomelo')

    wallet.api.transfer_to_savings('initminer', 'newaccount', Asset.Tbd(19.169), 'pomelo2')

    _result = wallet.api.get_account('newaccount')
    assert _result['balance'] == Asset.Test(5.432)
    assert _result['hbd_balance'] == Asset.Tbd(9.169)
    assert _result['savings_balance'] == Asset.Test(15.432)
    assert _result['savings_hbd_balance'] == Asset.Tbd(19.169)

    wallet.api.transfer_from_savings('newaccount', 7, 'newaccount2', Asset.Test(0.001), 'kiwi')

    wallet.api.transfer_from_savings('newaccount', 8, 'newaccount2', Asset.Tbd(0.001), 'kiwi2')

    _result = wallet.api.get_account('newaccount')
    assert _result['balance'] == Asset.Test(5.432)
    assert _result['hbd_balance'] == Asset.Tbd(9.169)
    assert _result['savings_balance'] == Asset.Test(15.431)
    assert _result['savings_hbd_balance'] == Asset.Tbd(19.168)

    response = wallet.api.transfer_nonblocking('newaccount', 'newaccount2', Asset.Test(0.1), 'mango')

    wallet.api.transfer_nonblocking('newaccount', 'newaccount2', Asset.Tbd(0.2), 'mango2')

    wallet.api.transfer_to_vesting_nonblocking('initminer', 'newaccount', Asset.Test(100))

    logger.info('Waiting...')
    node.wait_number_of_blocks(1)

    _result = wallet.api.get_account('newaccount')
    assert _result['balance'] == Asset.Test(5.332)
    assert _result['hbd_balance'] == Asset.Tbd(8.969)
    assert _result['savings_balance'] == Asset.Test(15.431)
    assert _result['vesting_shares'] != Asset.Vest(0)

    wallet.api.cancel_transfer_from_savings('newaccount', 7)

    response = wallet.api.get_account('newaccount')

    assert response['savings_balance'] == Asset.Test(15.432)
