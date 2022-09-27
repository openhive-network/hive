import pytest
import os
from collections import namedtuple
from urllib.parse import urlunparse

import test_tools as tt
from local_tools import Config, gather_all_markers, IS_DIRECT_CALL_HAFAH

# create this function only if run with tavern
if 'HIVEMIND_PORT' in os.environ:
    def pytest_tavern_beta_before_every_test_run(test_dict, variables):
        if IS_DIRECT_CALL_HAFAH:
            url = test_dict["stages"][0]["request"]["url"]
            method_name = test_dict["stages"][0]["request"]["json"]["method"].split(".")[1]
            test_dict["stages"][0]["request"]["url"] = f"{url}rpc/{method_name}"
            test_dict["stages"][0]["request"]["json"] = test_dict["stages"][0]["request"]["json"]["params"]


def pytest_configure(config):
    for marker in gather_all_markers():
        config.addinivalue_line(
            "markers", f"{marker}: extracted from *.tavern.yaml files, use for filtering tests"
        )

def pytest_addoption(parser):
    parser.addoption('--proto', type=str, default='http', help='Specifies protocol of connection to node [default: http]')
    parser.addoption('--address', type=str, default='127.0.0.1', help='Specifies address of node [default: 127.0.0.1]')
    parser.addoption('--port', type=int, default=8090, help='Specifies http port of node [default: 8090]')
    parser.addoption('--timeout', type=int, default=100, help='Specifies timeout for each requests [default: 100]')

@pytest.fixture
def config(request):
    components = namedtuple(
        typename='components',
        field_names=['scheme', 'netloc', 'url', 'path', 'query', 'fragment']
    )
    url = str(
        urlunparse(
            components(
                scheme=request.config.getoption('--proto'),
                netloc=f"{request.config.getoption('--address')}:{request.config.getoption('--port')}",
                query='',
                path='',
                url='',
                fragment=''
            )
        )
    )
    return Config(
        timeout=request.config.getoption('--timeout'),
        node=tt.RemoteNode(url),
        url=url
    )
