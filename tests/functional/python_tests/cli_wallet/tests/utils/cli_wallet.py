#!/usr/bin/python3

import sys
import json
import queue
import requests
import threading
import subprocess

from .logger   import log
from .cmd_args   import args
from .test_utils import last_message_as_json, ws_to_http

class CliWalletException(Exception):
  def __init__(self, _message):
    self.message = _message

  def __str__(self):
    return self.message  

class CliWallet(object):

  class CliWalletArgs(object):
    def __init__(self, args ):
      self.path = args.path+'/cli_wallet'
      self.server_rpc_endpoint = args.server_rpc_endpoint
      self.cert_auth = args.cert_auth
      #self.rpc_endpoint = _rpc_endpoint
      self.rpc_tls_endpoint = args.rpc_tls_endpoint
      self.rpc_tls_cert = args.rpc_tls_cert
      self.rpc_http_endpoint = args.rpc_http_endpoint
      self.deamon = args.deamon
      self.rpc_allowip = args.rpc_allowip
      self.wallet_file = args.wallet_file
      self.chain_id  = args.chain_id
      self.wif  = args.wif

    def args_to_list(self):
      test_args = []
      args = {"server_rpc_endpoint": self.server_rpc_endpoint}
      args["cert_auth"]     = self.cert_auth
      #args["rpc_endpoint"]    = self.rpc_endpoint
      args["rpc_tls_endpoint"]  = self.rpc_tls_endpoint
      args["rpc_tls_cert"]    = self.rpc_tls_cert
      args["rpc_http_endpoint"] =self.rpc_http_endpoint
      args["deamon"]      = self.deamon
      args["rpc_http_allowip"]     = self.rpc_allowip
      args["wallet_file"]     = self.wallet_file
      args["chain_id"]      = self.chain_id
      for key, val in args.items():
        if val:
          if isinstance(val, list):
            for _v in val:
              test_args.append("--"+key.replace("_","-")+ " ")
              test_args.append(_v)
            continue

          if not isinstance(val, bool):
            test_args.append("--"+key.replace("_","-"))
            test_args.append(val)
          elif key == 'deamon':
            test_args.append('-d')
      test_args = " ".join(test_args)
      return test_args


  def __init__(self, args):
    self.cli_args = CliWallet.CliWalletArgs(args)
    self.cli_proc = None
    self.response = ""


  def __getattr__(self, _method_name):
    if self.cli_proc:
      self.method_name = _method_name
      return self
    else:
      log.error("Cli_wallet is not set")
      raise CliWalletException("Cli_wallet is not set")


  def __call__(self,*_args):
    try:
      self.response = ""
      endpoint, params = self.prepare_args(*_args)
      self.response = self.send_and_read(endpoint, params)
      return self.response
    except Exception as _ex:
      log.exception("Exception `{0}` occurs while calling `{1}` with `{2}` args.".format(str(_ex), self.method_name, list(_args)))

  def __enter__(self):
    self.set_and_run_wallet()
    return self

  def __exit__(self, exc_type, exc_value, exc_traceback):
    try:
      from os import kill
      kill( self.cli_proc.pid, 9 )
    except:
      self.cli_proc.send_signal( subprocess.signal.SIGTERM ) # send ^C
      self.cli_proc.send_signal( subprocess.signal.SIGKILL ) # terminate
    finally:
      log.info("wallet closed")
    if exc_type:
      log.error(f"Error: {exc_value}")
      log.info(f"Traceback: {exc_traceback}")

  def set_and_run_wallet(self):
    from shlex import split as argsplitter
    try:
      print("Calling cli_wallet with args `{0}`".format([self.cli_args.path+ " " + self.cli_args.args_to_list()]))
      self.cli_proc = subprocess.Popen( args=[self.cli_args.path, *argsplitter(self.cli_args.args_to_list())], stdin=subprocess.DEVNULL, stderr=subprocess.DEVNULL  , stdout=subprocess.DEVNULL )
      if not self.cli_proc:
        raise CliWalletException("Failed to run cli_wallet")

      from time import sleep
      sleep(2)

      self.set_password("{0}".format("testpassword"))
      self.unlock("{0}".format("testpassword"))
      self.import_key("{0}".format(self.cli_args.wif))
    except Exception as _ex:
      log.exception("Exception `{0}` occuress while while running cli_wallet.".format(str(_ex)))


  #we dont have stansaction status api, so we need to combine...
  def wait_for_transaction_approwal(self):
    json_resp = last_message_as_json(self.response)
    block_num = json_resp["result"]["block_num"]
    trans_id  = json_resp["result"]["id"]
    url = ws_to_http(self.cli_args.server_rpc_endpoint)
    idx = -1
    while True:
      param = {"jsonrpc":"2.0", "method":"block_api.get_block", "params":{"block_num":block_num+idx}, "id":1}
      resp = requests.post(url, json=param)
      data = resp.json()
      if "result" in data and "block" in data["result"]:
        block_transactions = data["result"]["block"]["transaction_ids"]
        if trans_id in block_transactions:
          log.info("Transaction `{0}` founded in block `{1}`".format(trans_id, block_num+idx))
          break
      idx += 1


  def check_if_transaction(self):
    json_resp = last_message_as_json(self.response)
    if "result" in json_resp:
      if "id" in json_resp["result"]:
        return True
    return False

  def send(self, _data):
    self.cli_proc.stdin.write(_data.encode("utf-8"))
    self.cli_proc.stdin.flush()


  def send_and_read(self, _endpoint, _data):
    data = {
      "jsonrpc": "2.0", 
      "method": _endpoint,
      "params": _data,
      "id": 1
    }
    data = json.dumps( data )

    self.response = requests.post(
      f'http://{self.cli_args.rpc_http_endpoint}',
      data=data
    ).json()

    return self.response


  def exit_wallet(self):
    try:
      if not self.cli_proc:
        log.info("Cannot exit wallet, because wallet was not set - please run it first by using `run_wallet` metode.")
      self.cli_proc.communicate()
      return self.cli_proc
    except Exception as _ex:
      log.exception("Exception `{0}` occuress while while running cli_wallet.".format(str(_ex)))

  def prepare_args(self, *_args):
    from json import dumps
    return self.method_name, [ arg for arg in _args ]

