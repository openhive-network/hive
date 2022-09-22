from ..local_tools import prepare_escrow


def test_list_escrows(node, wallet):
    prepare_escrow(wallet, sender='alice')
    escrows = node.api.database.list_escrows(start=['alice', 0], limit=5, order='by_from_id')['escrows']
    assert len(escrows) != 0
