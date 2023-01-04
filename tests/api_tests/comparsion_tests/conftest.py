def pytest_addoption(parser):
    parser.addoption("--ref", action="store", type=str, help="specifies address of reference node")
    parser.addoption("--test", action="store", type=str, help="specifies address of tested service")
    parser.addoption("--start", action="store", type=int, default=4900000, help="specifies block range start")
    parser.addoption("--stop", action="store", type=int, default=4925000, help="specifies block range stop")
