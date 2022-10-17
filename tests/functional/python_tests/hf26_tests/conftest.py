
from pathlib import Path
from typing import Dict

import pytest

import test_tools as tt

from ..local_tools import prepare_sub_networks, allow_generate_block_log

def create_block_log_directory_name(name : str):
    return str(Path(__file__).parent.absolute()) + '/' + name

@pytest.fixture
def prepare_network_before_hf26() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([20], allow_generate_block_log(), create_block_log_directory_name('block_log_network_before_hf26')) }
