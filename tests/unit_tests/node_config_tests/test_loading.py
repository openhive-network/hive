import pytest

from test_tools.node_config import NodeConfig


@pytest.fixture
def config():
    return NodeConfig()


def test_single_value_loading(config):
    config.load_from_lines(['block-log-info-print-file = ILOG'])
    assert config.block_log_info_print_file == 'ILOG'


def test_incorrect_value_with_underscores_loading(config):
    with pytest.raises(KeyError):
        config.load_from_lines(['block_log_info_print_file = ILOG'])


def test_double_quoted_string_loading(config):
    config.load_from_lines([
        'account-history-rocksdb-path = "blockchain/account-history-rocksdb-storage"',
        'shared-file-dir = "blockchain"',
        'snapshot-root-dir = "snapshot"',
    ])

    # Output should not contain double quotes inside string
    assert config.account_history_rocksdb_path == 'blockchain/account-history-rocksdb-storage'
    assert config.shared_file_dir == 'blockchain'
    assert config.snapshot_root_dir == 'snapshot'


def test_correct_plugins(config):
    config.load_from_lines(['plugin = witness p2p account_by_key'])
    assert all(plugin in config.plugin for plugin in ['witness', 'p2p', 'account_by_key'])


def test_incorrect_plugins(config):
    for incorrect_plugin in ['UNDEFINED_PLUGIN', 'witnness', 'p3p']:
        with pytest.raises(ValueError):
            config.load_from_lines([f'plugin = {incorrect_plugin}'])
            pytest.fail(f'Exception was not raised for value: \"{incorrect_plugin}\"')


def test_single_line_entry_loading(config):
    config.load_from_lines([
        'plugin = account_by_key',
        'plugin = condenser_api',
    ])

    assert 'account_by_key' in config.plugin
    assert 'condenser_api' in config.plugin


def test_multi_line_entry_loading(config):
    config.load_from_lines([
        'witness = "initminer"',
        'witness = "other-witness"',
        'private-key = 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n',
        'private-key = 5JcCHFFWPW2DryUFDVd7ZXVj2Zo67rqMcvcq5inygZGBAPR1JoR',
    ])

    assert 'initminer' in config.witness
    assert 'other-witness' in config.witness
    assert '5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n' in config.private_key
    assert '5JcCHFFWPW2DryUFDVd7ZXVj2Zo67rqMcvcq5inygZGBAPR1JoR' in config.private_key


def test_entry_without_value_loading(config):
    config.load_from_lines(
        ['plugin = ']
    )

    assert not config.plugin


def test_if_entries_without_value_not_clears_previous(config):
    config.load_from_lines([
        'plugin = account_by_key',
        'plugin = condenser_api',
        'plugin = ',
    ])

    assert config.plugin == ['account_by_key', 'condenser_api']
