from hive_local_tools import run_for


@run_for('testnet')
def test_change_bucket_size_in_node_config(node):
    node.close()
    new_bucket_sizes = [1,2,3,4,5]
    node.config.market_history_bucket_size = str(new_bucket_sizes)
    node.run()
    response = node.api.market_history.get_market_history_buckets()
    assert response['bucket_sizes'] == new_bucket_sizes
