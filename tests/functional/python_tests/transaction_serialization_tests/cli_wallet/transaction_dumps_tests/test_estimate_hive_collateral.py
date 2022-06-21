import test_tools as tt


def test_estimate_hive_collateral(wallet):  #NIE ZWRACA NIC
    wallet.api.estimate_hive_collateral(tt.Asset.Tbd(10))
