def test_single_entry_serialization(config):
    config.required_participation = 0
    lines = config.write_to_lines()
    assert 'required-participation = 0' in lines


def test_multi_line_list_entry_serialization(config):
    config.private_key.extend(['A', 'B'])
    lines = config.write_to_lines()
    assert lines[0] == 'private-key = A'
    assert lines[1] == 'private-key = B'


def test_single_line_list_entry_serialization(config):
    config.plugin.extend(['p2p', 'witness'])
    lines = config.write_to_lines()
    assert lines[0] == 'plugin = p2p witness'
