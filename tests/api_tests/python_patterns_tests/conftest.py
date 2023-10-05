from test_tools.__private.scope.scope_fixtures import *


def pytest_addoption(parser):
    parser.addoption("--ws-endpoint", action="store", type=str, help="specifies ws_endpoint of reference node")
    parser.addoption("--wallet-path", action="store", type=str, help="specifies path of reference cli wallet")
