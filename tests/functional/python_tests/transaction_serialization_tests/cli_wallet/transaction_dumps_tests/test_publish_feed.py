import pytest


@pytest.mark.testnet
def test_publish_feed(wallet):
    feed_history = wallet.api.get_feed_history()
    current_median_history = feed_history['current_median_history']
    wallet.api.publish_feed('initminer', current_median_history)
