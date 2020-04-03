#!/usr/bin/python3

import json
import logging
import sys
import os
import common

from threading import Lock

MODULE_NAME = __file__

logger = logging.getLogger(MODULE_NAME)
logger.setLevel(common.DEFAULT_LOG_LEVEL)

ch = logging.StreamHandler(sys.stdout)
ch.setLevel(common.DEFAULT_LOG_LEVEL)
ch.setFormatter(logging.Formatter(common.DEFAULT_LOG_FORMAT))

logger.addHandler(ch)

from typing import NamedTuple

class HiveNode(object):
  hived_binary = None
  hived_process = None
  hived_lock = Lock()
  hived_data_dir = None
  hived_args = list()

  def __init__(self, binary_path : str, working_dir : str, binary_args : list):
    logger.info("New hive node")
    if not os.path.exists(binary_path):
      raise ValueError("Path to hived binary is not valid.")
    if not os.path.isfile(binary_path):
      raise ValueError("Path to hived binary must point to file")
    self.hived_binary = binary_path

    if not os.path.exists(working_dir):
      raise ValueError("Path to data directory is not valid")
    if not os.path.isdir(working_dir):
      raise ValueError("Data directory is not valid directory")
    self.hived_data_dir = working_dir

    if binary_args:
      self.hived_args.extend(binary_args)

  def __enter__(self):
    self.hived_lock.acquire()

    from subprocess import Popen, PIPE
    from time import sleep

    hived_command = [
      self.hived_binary,
      "--data-dir={}".format(self.hived_data_dir)
    ]
    hived_command.extend(self.hived_args)

    self.hived_process = Popen(hived_command, stdout=None, stderr=None)
    self.hived_process.poll()
    sleep(5)

    if self.hived_process.returncode:
      raise Exception("Error during starting node")

  def __exit__(self, exc, value, tb):
    logger.info("Closing node")
    from signal import SIGINT, SIGTERM
    from time import sleep

    if self.hived_process is not None:
      self.hived_process.poll()
      if self.hived_process.returncode != 0:
        self.hived_process.send_signal(SIGINT)
        sleep(7)
        self.hived_process.poll()
        if self.hived_process.returncode != 0:
          self.hived_process.send_signal(SIGTERM)
          sleep(7)
          self.hived_process.poll()
          if self.hived_process.returncode != 0:
            raise Exception("Error during stopping node. Manual intervention required.")
    self.hived_process = None
    self.hived_lock.release()

if __name__ == "__main__":
  KEEP_GOING = True

  def sigint_handler(signum, frame):
    logger.info("Shutting down...")
    global KEEP_GOING
    from time import sleep
    KEEP_GOING = False
    sleep(3)
    sys.exit(0)

  def main():
    try:
      import signal
      signal.signal(signal.SIGINT, sigint_handler)

      plugins = ["chain","p2p","webserver","json_rpc","debug_node"]
      config = "# Simple config file\n" \
        + "shared-file-size = 1G\n" \
        + "enable-stale-production = true\n" \
        + "p2p-endpoint = 127.0.0.1:2001\n" \
        + "webserver-http-endpoint = 127.0.0.1:8095\n" \
        + "webserver-ws-endpoint = 127.0.0.1:8096\n" \
        + "plugin = witness debug_node {}\n".format(" ".join(plugins)) \
        + "plugin = database_api debug_node_api block_api\n" \
        + "witness = \"initminer\"\n" \
        + "private-key = 5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n\n" \
        + "required-participation = 0"

      binary_path = "/home/dariusz-work/Builds/hive/programs/steemd/steemd"
      work_dir = "/home/dariusz-work/hive-data"

      print(config)

      with open(work_dir + "/config.ini", "w") as conf_file:
        conf_file.write(config)

      node = HiveNode(binary_path, work_dir, [])
      from time import sleep
      from common import wait_n_blocks, debug_generate_blocks
      with node:
        print("Waiting 10 blocks")
        wait_n_blocks("http://127.0.0.1:8095", 10)
        print("Done...")
        print(debug_generate_blocks("http://127.0.0.1:8095", "5JHNbFNDg834SFj8CMArV6YW7td4zrPzXveqTfaShmYVuYNeK69", 100))
        while(KEEP_GOING):
          sleep(1)
    except Exception as ex:
      logger.exception("Exception: {}".format(ex))
      sys.exit(1)
  
  main()



