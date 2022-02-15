from test_tools import RemoteNode, Wallet, Asset
from test_tools.communication import request


def test_x():
    node = RemoteNode('51.255.65.133:8091', ws_endpoint='51.255.65.133:8090')
    wallet = Wallet(attach_to=node)

    wallet.api.import_key('5Huf9VJXoSzLFw6GPfcCP9fFjR6vsAjZyeRykgJ8DcECng2Fd3Y ')

    info = wallet.api.info()
    # x = wallet.api.get_account('kudmich')
    # wallet.api.sign_transaction(x, broadcast=False)

    witness = wallet.api.get_active_witnesses()
    # witness2 = wallet.api.list_witnesses(start='abit', limit=100, order='by_name')
    # acc_list = wallet.api.list_accounts(-1, 1000)
    print()
