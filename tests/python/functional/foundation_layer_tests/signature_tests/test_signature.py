from __future__ import annotations

import test_tools as tt


def test_signatures():
    network = tt.Network()

    init_node = tt.InitNode(network=network)
    api_node = tt.ApiNode(network=network)

    network.run()
    wallet = tt.Wallet(attach_to=init_node)
    wallet.create_account("alice", hives=tt.Asset.Test(1000), vests=tt.Asset.Test(1000), hbds=tt.Asset.Tbd(3000))
    print()
