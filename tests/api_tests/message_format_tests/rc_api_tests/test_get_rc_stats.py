import pytest
from hive_local_tools import run_for

@run_for('live_mainnet')
def test_get_rc_stats(node):
    node.api.rc.get_rc_stats()


@run_for('live_mainnet')
def test_get_rc_stats_with_additional_argument(node):
    with pytest.raises(ValueError):
        node.api.rc.get_rc_stats("additional_argument")


@run_for('live_mainnet')
def test_get_rc_stats_with_additional_kwarg_argument(node):
    node.api.rc.get_rc_stats(additional_argument="some_value")
