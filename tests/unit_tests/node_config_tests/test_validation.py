import pytest


def test_detection_of_duplicated_plugins(config):
    config.plugin.extend(['condenser_api', 'condenser_api'])
    with pytest.raises(RuntimeError):
        config.validate()
