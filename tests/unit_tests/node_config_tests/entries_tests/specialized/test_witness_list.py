import pytest

from test_library.node_config import NodeConfig
from test_library import Account


@pytest.fixture
def config():
    return NodeConfig()


@pytest.fixture
def witness():
    return Account('initminer')


@pytest.fixture
def witnesses():
    return [Account(witness) for witness in ['witness0', 'witness1', 'witness2']]


def test_witness_appending_with_private_key_registration(config, witness):
    config.witness.append(witness.name)
    assert witness.private_key in config.private_key


def test_witness_appending_without_private_key_registration(config, witness):
    config.witness.append(witness.name, with_key=False)
    assert witness.private_key not in config.private_key


def test_witness_extending_with_private_key_registration(config, witnesses):
    config.witness.extend([witness.name for witness in witnesses])
    assert all([witness.private_key in config.private_key for witness in witnesses])


def test_witness_extending_without_private_key_registration(config, witnesses):
    config.witness.extend([witness.name for witness in witnesses], with_key=False)
    assert all([witness.private_key not in config.private_key for witness in witnesses])
