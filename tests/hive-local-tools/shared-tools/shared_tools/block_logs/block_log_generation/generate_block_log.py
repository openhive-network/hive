from pathlib import Path

import test_tools as tt

from shared_tools.complex_networks import generate_networks
import shared_tools.networks_architecture as networks

def prepare_blocklog():

    # Before creating `config` take a look at `README.md`
    config = {}

    architecture = networks.NetworksArchitecture()
    architecture.load(config)

    tt.logger.info(architecture)

    generate_networks(architecture, Path('generated'))

if __name__ == "__main__":
    prepare_blocklog()
