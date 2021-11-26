from test_tools import logger, World, Wallet
from pathlib import Path
import subprocess
import re

def test_dump_config(world : World):
  database = "postgresql://dev:devdevdev@localhost:5432/hivemind_inte"

  node = world.create_init_node('init_0')
  node.run()
  logger.info(f'http_endpoint = {node.get_http_endpoint()}')
  http_endpoint = node.get_http_endpoint()
  http_endpoint_clean = http_endpoint.replace("'","")
  node.wait_number_of_blocks(10)
  process = subprocess.Popen(
    [
      'hive',
      'sync',
      "--database-url=" + database,
      "--steemd-url={" + '"default" : "' + http_endpoint_clean + '"}'

    ]
  )
  node.wait_number_of_blocks(20)
  node.close()

  # process = subprocess.Popen(
  #   [
  #     'hive',
  #     'sync',
  #     "--steemd-url='{" + '"default" : "' + http_endpoint + '"}' + "'"
  #
  #   ]
  #
  # cwd = self.directory,
  #       stdout = self.stdout_file,
  #                stderr = self.stderr_file
  # )

# "--steemd-url='{" + '"default" : "' + http_endpoint_clean + '"}' + "'"
