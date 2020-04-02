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

class ConfigObject(NamedTuple):
  path_to_binary : str
  binary_args : list
  data_dir :str
  plugins : list
  p2p_endpoint : str
  http_endpoint : str
  rpc_endpoint : str
  witness : str
  private_key : str

def make_config_ini(config_object : ConfigObject):
  return "# Simple config file\n" \
    + "shared-file-size = 1G\n" \
    + "enable-stale-production = true\n" \
    + "p2p-endpoint = {}\n".format(config_object.p2p_endpoint) \
    + "webserver-http-endpoint = {}\n".format(config_object.http_endpoint) \
    + "webserver-ws-endpoint = {}\n".format(config_object.rpc_endpoint) \
    + "enable-plugin = witness debug_node {}\n".format(" ".join(config_object.plugins)) \
    + "witness = \"{}\"\n".format(config_object.witness) \
    + "private-key = {}\n".format(config_object.private_key) \
    + "required-participation = 0"

class HiveNode(object):
  hived_binary = None
  hived_process = None
  hived_lock = Lock()
  hived_data_dir = None
  hived_args = list()

  def __init__(self, config_object : ConfigObject, create_config_ini = False):
    logger.info("New hive node")
    if not os.path.exists(config_object.path_to_binary):
      raise ValueError("Path to hived binary is not valid.")
    if not os.path.isfile(config_object.path_to_binary):
      raise ValueError("Path to hived binary must point to file")
    self.hived_binary = config_object.path_to_binary

    if not os.path.exists(config_object.data_dir):
      raise ValueError("Path to data directory is not valid")
    if not os.path.isdir(config_object.data_dir):
      raise ValueError("Data directory is not valid directory")
    self.hived_data_dir = config_object.data_dir

    if create_config_ini:
      with open(self.hived_data_dir + "/config.ini", "w") as confg_file:
        confg_file.write(make_config_ini(config_object))

    if config_object.binary_args:
      self.hived_args.extend(config_object.binary_args)

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

KEEP_GOING = True

def sigint_handler(signum, frame):
  logger.info("Shutting down...")
  global KEEP_GOING
  from time import sleep
  KEEP_GOING = False
  sleep(3)
  sys.exit(0)

if __name__ == "__main__":
  import signal
  signal.signal(signal.SIGINT, sigint_handler)

  c = ConfigObject(
    "/home/dariusz-work/Builds/hive/programs/steemd/steemd",
    [],
    "/home/dariusz-work/hive-data",
    ["chain","p2p","database_api","webserver","network_broadcast_api","block_api","json_rpc"],
    "127.0.0.1:2001",
    "127.0.0.1:8095",
    "127.0.0.1:8096",
    "initminer",
    "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n"
  )

  print(make_config_ini(c))

  node = HiveNode(c, True)
  from time import sleep
  with node:
    while(KEEP_GOING):
      sleep(1)



