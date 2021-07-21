import pytest
from pathlib import Path

def pytest_addoption(parser):
    parser.addoption("--block_log", action="store", default="block_log")


@pytest.fixture()
def block_log(pytestconfig):
    str = pytestconfig.getoption("block_log")
    return Path(str)
