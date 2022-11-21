from pathlib import Path
from typing import Dict

import pytest

from shared_tools.complex_networks import (allow_generate_block_log,
                                           prepare_network_with_many_witnesses,
                                           prepare_sub_networks)


def create_block_log_directory_name(name : str):
    return str(Path(__file__).parent.absolute()) + '/' + name

@pytest.fixture
def prepare_basic_networks() -> Dict:
    alpha_net, beta_net = prepare_network_with_many_witnesses(create_block_log_directory_name('block_log'))
    yield {
        'Alpha': alpha_net,
        'Beta': beta_net,
    }

@pytest.fixture
def prepare_sub_networks_10_11() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([10, 11]) }

@pytest.fixture
def prepare_fork_2_sub_networks_00() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([3, 18], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_2_sub_networks_00')) }

@pytest.fixture
def prepare_fork_2_sub_networks_01() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([6, 17], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_2_sub_networks_01')) }

@pytest.fixture
def prepare_fork_2_sub_networks_02() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([6, 17], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_2_sub_networks_02')) }

@pytest.fixture
def prepare_fork_2_sub_networks_03() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([3, 18], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_2_sub_networks_03')) }

@pytest.fixture
def prepare_obi_throw_exception_00() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([10, 11], allow_generate_block_log(), create_block_log_directory_name('block_log_obi_throw_exception_00')) }

@pytest.fixture
def prepare_obi_throw_exception_01() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([10, 11], allow_generate_block_log(), create_block_log_directory_name('block_log_obi_throw_exception_01')) }

@pytest.fixture
def prepare_obi_throw_exception_02() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([10, 11], allow_generate_block_log(), create_block_log_directory_name('block_log_obi_throw_exception_02')) }

@pytest.fixture
def prepare_fork_3_sub_networks_00() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([3, 4, 14], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_3_sub_networks_00')) }

@pytest.fixture
def prepare_fork_3_sub_networks_01() -> Dict:
    yield { 'sub-networks-data': prepare_sub_networks([7, 7, 7], allow_generate_block_log(), create_block_log_directory_name('block_log_fork_3_sub_networks_01')) }
