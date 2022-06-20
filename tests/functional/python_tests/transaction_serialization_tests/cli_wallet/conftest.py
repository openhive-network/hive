import logging


def pytest_addoption(parser):
    parser.addoption("--http-endpoint", action="store", type=str, help='specifies http_endpoint of reference node')
    parser.addoption("--ws-endpoint", action="store", type=str, help='specifies ws_endpoint of reference node')
    parser.addoption("--wallet-path", action="store", type=str, help='specifies path of reference cli wallet')


def pytest_sessionstart() -> None:
    # Turn off unnecessary logs
    logging.getLogger('urllib3.connectionpool').propagate = False
