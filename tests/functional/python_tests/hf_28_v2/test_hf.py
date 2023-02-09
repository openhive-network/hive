def test_hf_27(node_hf27):
    hf = node_hf27.api.condenser.get_version()["blockchain_version"]
    dhf = node_hf27.api.database.get_version()["blockchain_version"]
    hf2 = node_hf27.api.condenser.get_hardfork_version()
    print()

def test_hf_28(node_hf28):
    hf = node_hf28.api.condenser.get_version()["blockchain_version"]
    dhf = node_hf28.api.database.get_version()["blockchain_version"]
    hf2 = node_hf28.api.condenser.get_hardfork_version()
    print()

def test_hf_272(node):
    con_hv = node.api.condenser.get_version()["blockchain_version"]
    dat_hv = node.api.database.get_version()["blockchain_version"]
    hf2 = node.api.condenser.get_hardfork_version()
    print()