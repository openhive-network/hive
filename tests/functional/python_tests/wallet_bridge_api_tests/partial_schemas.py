ASSET_HBD = {
    'type': 'map',
    'mapping': {
        'amount': {'type': 'str'},
        'precision': {'type': 'int', 'enum': [3]},
        'nai': {'type': 'str', 'pattern': '@@000000013'},
    }
}

ASSET_HIVE = {
    'type': 'map',
    'mapping': {
        'amount': {'type': 'str'},
        'precision': {'type': 'int', 'enum': [3]},
        'nai': {'type': 'str', 'pattern': '@@000000021'},
    }
}

ASSET_VESTS = {
    'type': 'map',
    'mapping': {
        'amount': {'type': 'str'},
        'precision': {'type': 'int', 'enum': [6]},
        'nai': {'type': 'str', 'pattern': '@@000000037'},
    }
}
