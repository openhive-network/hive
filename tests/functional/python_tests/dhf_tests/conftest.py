import pytest
import hive_utils
import logging

logger = logging.getLogger()

def pytest_addoption(parser):
  parser.addoption("--creator", help = "Account to create proposals with")
  parser.addoption("--receiver", help = "Account to receive funds")
  parser.addoption("--wif", help="Private key for creator account")
  parser.addoption("--node-url", default="http://127.0.0.1:8090", help="Url of working hive node")
  parser.addoption("--run-hived", help = "Path to hived executable. Warning: using this option will erase contents of selected hived working directory.")
  parser.addoption("--working-dir", default="/tmp/hived-data/", help = "Path to hived working directory")
  parser.addoption("--config-path", default="../../hive_utils/resources/config.ini.in",help = "Path to source config.ini file")
  parser.addoption("--no-erase-proposal", action='store_true', help = "Do not erase proposal created with this test")
  

@pytest.fixture(scope="module")
def hive_node_provider(pytestconfig):
  """ Starts and stops hived node """
  # start node
  node = None
  hived_path = pytestconfig.getoption('--run-hived', None)
  assert hived_path is not None, '--run-hived option not set'
  hived_working_dir = pytestconfig.getoption('--working-dir', None)
  assert hived_working_dir, '--working-dir option not set'
  hived_config_path = pytestconfig.getoption('--config-path', None)
  assert hived_config_path, '--config-path option not set'
  logger.info("Running hived via {} in {} with config {}".format(hived_path, 
      hived_working_dir, 
      hived_config_path)
  )
  
  node = hive_utils.hive_node.HiveNodeInScreen(
      hived_path, 
      hived_working_dir, 
      hived_config_path
  )
  node.run_hive_node(["--enable-stale-production"])
  assert node is not None, "Node is None"
  assert node.is_running(), "Node is not running"
  yield node
  logger.info("Stop hived node")
  node.stop_hive_node()