import pytest
import test_tools as tt


@pytest.fixture
def remote_node():
    remote_node = tt.RemoteNode(http_endpoint="http://hive-staging.pl.syncad.com:58091")
    return remote_node
