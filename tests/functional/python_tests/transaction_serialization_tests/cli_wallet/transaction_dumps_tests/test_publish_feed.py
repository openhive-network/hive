import test_tools as tt


def test_publish_feed(wallet):
    wallet.api.publish_feed('initminer', {"base": f"{tt.Asset.Tbd(1)}", "quote": f"{tt.Asset.Test(2)}"})
