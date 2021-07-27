import pytest

from test_tools import Asset


def test_warning_about_losing_precision():
    with pytest.warns(UserWarning):
        Asset.Hive(0.0001)
