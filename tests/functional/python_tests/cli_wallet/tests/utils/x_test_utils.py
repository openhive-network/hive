import json
import requests
from contextlib import ContextDecorator
import traceback
import time

from junit_xml import TestCase, TestSuite

from utils.logger     import log, init_logger
from utils.cmd_args   import args

class ArgsCheckException(Exception):
    def __init__(self, _message):
        self.message = _message

    def __str__(self):
        return self.message

class Asset:
  def __init__( self, am : float, dec : int = None, sym : str = None ):
    if dec is None and sym is None:
      am, dec, sym = self.__parse_str(am)

    self.amount = am
    self.decimals = dec
    self.symbol = sym

  def to_string(self):
    return "{1:.{0}f} {2}".format(self.decimals, self.amount, self.symbol)

  # returns amount, decimals and symbol
  def __parse_str( self, _in : str ):
    _in = _in.strip()
    amount, symbol = _in.split(' ')
    _, decimals = amount.split('.')
    amount = float(amount)
    return amount, len(decimals), symbol

  def __str__(self):
    return self.to_string()

  def satoshi(self):
    return Asset( 10 ** ( -1 * 3 ), self.decimals, self.symbol )

user_name = list("aaaaaaaaaaaa")
creator = "initminer"



def unifie_to_string(_arg):
    if not isinstance(_arg, str):
        prepared = str(_arg)
    else:
        prepared = _arg
    prepared = prepared.strip()
    prepared = prepared.replace(" ", "")
    return prepared


def check_call_args(_call_args, _response, _arg_prefix):
    splited_response = _response.split()
    for idx, line in enumerate(splited_response):
        if _arg_prefix in line:
            key = line[line.find(_arg_prefix+".")+len(_arg_prefix+"."):-1]
            #unifie to string
            call = unifie_to_string(_call_args[key])
            spli = unifie_to_string(splited_response[idx+1])

            if call in spli:
                _call_args.pop(key)
            else:
                log.error("Call arg `{0}` expected `{1}`".format(_call_args[key], str(splited_response[idx+1])))
                raise ArgsCheckException("Incossisten value for `{0}` key".format(key))
            if not _call_args:
                break
    if _call_args:
        raise ArgsCheckException("No all values checked, those `{0}` still remains. ".format(_call_args))


def call_and_check(_func, _call_args, _arg_prefix):
    response = _func(*_call_args.values())
    log.info("Call response: {0}".format(response))
    check_call_args(_call_args, response, _arg_prefix)
    return response


def call_and_check_transaction(_func, _call_args, _arg_prefix, _broadcast):
    response = _func(*_call_args.values(), _broadcast)
    check_call_args(_call_args, response, _arg_prefix)


def last_message_as_json( _message):
  if isinstance(_message, dict):
    return _message
  log.info(_message)
  if "message:" in _message:
      _message = _message[_message.rfind("message:")+len("message:"):]
      _message.strip()
      o = 0
      #lame... but works
      for index, ch in enumerate(_message):
          if str(ch) == "{":
              o +=1
              continue
          if str(ch) == "}":
              o -=1
              if o == 0:
                  _message = _message[:index+1]
                  break
  else:
      _message = "{}"
  return json.loads(_message)


def find_creator_proposals(_creator, _proposal_list):
    proposals = []
    if "result" in _proposal_list:
        result = _proposal_list["result"]
        if result:
            for rs in result:
                if rs["creator"] == _creator:
                    proposals.append(rs)
    return proposals


def find_voter_proposals(_voter, _proposal_list):
    proposals = []
    if "result" in _proposal_list:
        result = _proposal_list["result"]
        if result:
            for rs in result:
                if rs["voter"] == _voter:
                    proposals.append(rs)
    return proposals


def ws_to_http(_url):
    pos = _url.find(":")
    return "http" + _url[pos:]


def get_valid_hive_account_name():
    http_url = args.server_http_endpoint
    while True:
        params = {"jsonrpc":"2.0", "method":"condenser_api.get_accounts", "params":[["".join(user_name)]], "id":1}
        resp = requests.post(http_url, json=params)
        data = resp.json()
        if "result" in data:
            if len(data["result"]) == 0:
                return ''.join(user_name)
        if ord(user_name[0]) >= ord('z'):
            for i, _ in enumerate("".join(user_name)):
                if user_name[i] >= 'z':
                    user_name[i] = 'a'
                    continue
                user_name[i] = chr(ord(user_name[i]) + 1)
                break
        else:
            user_name[0] = chr(ord(user_name[0]) + 1)
        if len(set(user_name)) == 1 and user_name[0] == 'z':
            break

# if all_diffrent: { "owner": {"pub", "prv"}, "active": {"pub", "prv"}, "posting": {"pub", "prv"}, "memo": {"pub", "prv"} }
# if not all_diffrent: {"pub", "prv"}
def get_keys( cli_wallet, import_to_wallet : bool = True, all_diffrent : bool = False) -> dict:
  def _get_prv_pub_pair() -> dict:
    keys = cli_wallet.suggest_brain_key()['result']
    if import_to_wallet:
      cli_wallet.import_key( keys['wif_priv_key'] )
    return { "pub": keys['pub_key'], "prv": keys['wif_priv_key'] }

  if all_diffrent:
    return {
      "owner": _get_prv_pub_pair(),
      "active": _get_prv_pub_pair(),
      "posting": _get_prv_pub_pair(),
      "memo": _get_prv_pub_pair()
    }
  else:
    return _get_prv_pub_pair()

def make_user_for_tests(_cli_wallet, _value_for_vesting = "20.000 TESTS",  _value_for_transfer_tests = "20.000 TESTS", _value_for_transfer_tbd = "20.000 TBD"):
    receiver = get_valid_hive_account_name()

    print(_cli_wallet.create_account( creator, receiver, "{}", "true"))

    _cli_wallet.transfer_to_vesting(    creator, receiver, _value_for_vesting, "true")
    _cli_wallet.transfer(               creator, receiver, _value_for_transfer_tests, "initial transfer", "true" )
    _cli_wallet.transfer(               creator, receiver, _value_for_transfer_tbd, "initial transfer", "true")

    return creator, receiver

def create_users( _cli_wallet, count ) -> dict:
  ret = dict()
  for _ in range(count):
    name = get_valid_hive_account_name() 
    keys = get_keys( _cli_wallet )
    pub = keys['pub']
    ret[name] = keys
    _cli_wallet.create_account_with_keys( creator, name, "{}", pub, pub, pub, pub, "true" )
  return ret

class Test(ContextDecorator):
    def __init__(self, _test_name):
        self.test_name=_test_name

    def __enter__(self):
        init_logger(self.test_name)
        log.info("Starting test: {0}".format(self.test_name))

    def __exit__(self, exc_type, exc_value, exc_traceback):
        self.error=None
        if exc_type:
            log.exception(exc_value)
            self.error = traceback.format_exception(exc_type, exc_value, exc_traceback)
        if self.error:
            log.error("TEST `{0}` failed".format(self.test_name))
            raise Exception(str(self.error))
        else:
            log.info("TEST `{0}` passed".format(self.test_name))
            exit(0)

