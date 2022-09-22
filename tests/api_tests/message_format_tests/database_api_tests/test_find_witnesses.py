def test_find_witnesses(node, wallet):
    witnesses = node.api.database.find_witnesses(owners=['initminer'])['witnesses']
    assert len(witnesses) != 0
