def pytest_configure(config):
    config.addinivalue_line('markers', 'node_shared_file_size: Shared file size of node from `node` fixture')
