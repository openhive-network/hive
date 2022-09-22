from ..local_tools import prepare_escrow


def test_find_escrows(node, wallet):
    prepare_escrow(wallet, sender='alice')
    # "from" is a Python keyword and needs workaround
    escrows = node.api.database.find_escrows(**{'from': 'alice'})['escrows']
    assert len(escrows) != 0
