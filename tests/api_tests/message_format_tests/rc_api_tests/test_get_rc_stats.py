import pytest


def test_get_rc_stats(remote_node):
    remote_node.api.rc.get_rc_stats()


def test_get_rc_stats_with_additional_argument(remote_node):
    with pytest.raises(ValueError):
        remote_node.api.rc.get_rc_stats("argument")


def test_get_rc_stats_with_additional_kwarg_argument(remote_node):
    remote_node.api.rc.get_rc_stats(additional="argument")
