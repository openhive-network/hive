#!/usr/bin/python3

import time

import os
import sys
import json

import random

sys.path.append("../../../tests_api")
from jsonsocket import hived_call
from hive.steem.client import SteemClient

# TODO: Remove dependency from cli_wallet/tests directory.
#       This modules [utils.logger] should be somewhere higher.
sys.path.append("../cli_wallet/tests")
from utils.logger     import log, init_logger

class Client(SteemClient):
  def __init__(self, url):
    super(Client, self).__init__(url)

  def steemd_call(self, method, method_api, params):
    result = self._SteemClient__exec(method, params, method_api)
    return (True, result)


def set_password(_url):
    log.info("Call to set_password")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "set_password",
      "params": ["testpassword"]
      } ), "utf-8" ) + b"\r\n"

    status, response = hived_call(_url, data=request)
    return status, response

def unlock(_url):
    log.info("Call to unlock")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "unlock",
      "params": ["testpassword"]
      } ), "utf-8" ) + b"\r\n"

    status, response = hived_call(_url, data=request)
    return status, response

def import_key(_url, k):
    log.info("Call to import_key")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "import_key",
      "params": [k]
      } ), "utf-8" ) + b"\r\n"

    status, response = hived_call(_url, data=request)
    return status, response

def checked_hived_call(_url, data):
  status, response = hived_call(_url, data)
  if status == False or response is None or "result" not in response:
    log.error("Request failed: {0} with response {1}".format(str(data), str(response)))
    raise Exception("Broken response for request {0}: {1}".format(str(data), str(response)))

  return status, response

def wallet_call(_url, data):
  unlock(_url)
  status, response = checked_hived_call(_url, data)

  return status, response

def get_node_id(_url):
    log.info("Fetching node_id from {0}".format(str(_url)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "network_node_api.get_info"
    } ), "utf-8" ) + b"\r\n"

    status, response = checked_hived_call(_url, data=request)
    return response["result"]["node_id"]

def set_allowed_peers(_url, peers=[]):
    log.info("Calling set_allowed_peers for {0}".format(str(_url)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "network_node_api.set_allowed_peers",
      "params": {
        "allowed_peers": peers
      }
    } ), "utf-8" ) + b"\r\n"


    log.info(str(request))

    status, response = checked_hived_call(_url, data=request)
    return status

  

def create_account(_url, _creator, account):
    _name = account["account_name"]
    _pubkey = account["public_key"]
    _privkey = account["private_key"]

    log.info("Call to create account {0} {1}".format(str(_creator), str(_name)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "create_account_with_keys",
      "params": [_creator, _name, "", _pubkey, _pubkey, _pubkey, _pubkey, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)
    status, response = import_key(_url, _privkey)

def transfer(_url, _sender, _receiver, _amount, _memo):
    log.info("Attempting to transfer from {0} to {1}, amount: {2}".format(str(_sender), str(_receiver), str(_amount)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "transfer",
      "params": [_sender, _receiver, _amount, _memo, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def transfer_to_vesting(_url, _sender, _receiver, _amount):
    log.info("Attempting to transfer_to_vesting from {0} to {1}, amount: {2}".format(str(_sender), str(_receiver), str(_amount)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "transfer_to_vesting",
      "params": [_sender, _receiver, _amount, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def set_voting_proxy(_account, _proxy, _url):
    log.info("Attempting to set account `{0} as proxy to {1}".format(str(_proxy), str(_account)))
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "set_voting_proxy",
      "params": [_account, _proxy, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def vote_for_witness(_account, _witness, _approve, _url):
  if(_approve):
    log.info("Account `{0} votes for wittness {1}".format(str(_account), str(_witness)))
  else:
    log.info("Account `{0} revoke its votes for wittness {1}".format(str(_account), str(_witness)))

  request = bytes( json.dumps( {
    "jsonrpc": "2.0",
    "id": 0,
    "method": "vote_for_witness",
    "params": [_account, _witness, _approve, 1]
    } ), "utf-8" ) + b"\r\n"

  status, response = wallet_call(_url, data=request)

def vote_for_witnesses(_account, _witnesses, _approve, _url):
  for w in _witnesses:
    if(isinstance(w, str)):
      vote_for_witness(_account, w, 1, _url)
    else:
      vote_for_witness(_account, w["account_name"], 1, _url)

def register_witness(_url, _account_name, _witness_url, _block_signing_public_key):
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "update_witness",
      "params": [_account_name, _witness_url, _block_signing_public_key, {"account_creation_fee": "3.000 TESTS","maximum_block_size": 65536,"sbd_interest_rate": 0}, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def unregister_witness(_url, _account_name):
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "update_witness",
      "params": [_account_name, "https://heaven.net", "STM1111111111111111111111111111111114T1Anm", {"account_creation_fee": "3.000 TESTS","maximum_block_size": 65536,"sbd_interest_rate": 0}, 1]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

def info(_url):
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "info",
      "params": []
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(_url, data=request)

    return response["result"]

def get_amount(_asset):
  amount, symbol = _asset.split(" ")
  return float(amount)

def self_vote(_witnesses, _url):
  for w in _witnesses:
    if(isinstance(w, str)):
      vote_for_witness(w, w, 1, _url)
    else:
      vote_for_witness(w["account_name"], w["account_name"], 1, _url)

def prepare_accounts(_accounts, _url):
    log.info("Attempting to create {0} accounts".format(len(_accounts)))
    for account in _accounts:
        create_account(_url, "initminer", account)

def configure_initial_vesting(_accounts, _tests, _url):
    log.info("Attempting to prepare {0} of witnesses".format(str(len(_accounts))))
    for account in _accounts:
        if isinstance(account, str):
          account_name = account
        else:
          account_name = account["account_name"]
        transfer_to_vesting(_url, "initminer", account_name, _tests)

def prepare_witnesses(_witnesses, _url):
    log.info("Attempting to prepare {0} of witnesses".format(str(len(_witnesses))))
    for witness in _witnesses:
        account_name = witness["account_name"]
        pub_key = witness["public_key"]
        register_witness(_url, account_name, "https://" + account_name + ".net", pub_key)

def synchronize_balances(_accounts, _url, _mainNetUrl):
  log.info("Attempting to synchronize balances of {0} accounts".format(str(len(_accounts))))

  accountList = []

  for a in _accounts:
    accountList.append(a["account_name"])

  client = Client(_mainNetUrl)
  accounts = client.get_accounts(accountList)

  log.info("Successfully retrieved info for {0} accounts".format(str(len(accounts))))

  for a in accounts:
    name = a["name"]
    vests = a["vesting_shares"]
    steem = a["balance"]
    sbd = a["sbd_balance"]

    log.info("Account {0} balances: `{1}', `{2}', `{3}'".format(str(name), str(steem), str(sbd), str(vests)))

    steem_amount, steem_symbol = steem.split(" ")

    if(float(steem_amount) > 0):
      tests = steem.replace("STEEM", "TESTS")
      transfer(_url, "initminer", name, tests, "initial balance sync")

    sbd_amount, sbd_symbol = sbd.split(" ")
    if(float(sbd_amount) > 0):
      tbd = sbd.replace("SBD", "TBD")
      transfer(_url, "initminer", name, tbd, "initial balance sync")

def list_accounts(url):
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "list_accounts",
      "params": ["", 100]
      } ), "utf-8" ) + b"\r\n"

    status, response = wallet_call(url, data=request)
    print(response)

def list_top_witnesses(_url):
    start_object = [200277786075957257, ""]
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "database_api.list_witnesses",
      "params": {
        "limit": 100,
        "order": "by_vote_name",
        "start": start_object
      }
    } ), "utf-8" ) + b"\r\n"

    status, response = checked_hived_call(_url, data=request)
    return response["result"]["witnesses"]

def print_top_witnesses(witnesses, api_node_url):

  witnesses_set = set()
  for w in witnesses:
    witnesses_set.add(w["account_name"])

  top_witnesses = list_top_witnesses(api_node_url)
  position = 1
  for w in top_witnesses:
    owner = w["owner"]
    group = "U"
    if (owner in witnesses_set):
      group = "W"

    log.info("Witness # {0:2d}, group: {1}, name: `{2}', votes: {3}".format(position, group, w["owner"], w["votes"]))
    position = position + 1


# First start wallet as follows:
# ./cli_wallet --chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39 -s ws://0.0.0.0:8090 -d -H 0.0.0.0:8093  --rpc-http-allowip 192.168.10.10 --rpc-http-allowip=127.0.0.1

# That is, our testnet will have 22 witnesses running the code that will HF, and and 20 that won't HF (simulating Justin's sockpuppets).
# We will have 10 running the HF code in the top 20, 10 of the sockpuppets in the top 20, and 12 running the HF below those groups.
# The vote weighting should be arranged so that at the time of the HF, the sockpuppets will drop out of the top 20 because of lost vote power
# and get replaced by the 12 other HF witnesses that were ranked below them before

if __name__ == "__main__":
    try:
        init_logger(os.path.abspath(__file__))


        witnesses = [
          {"private_key":"5Jk6EPaz94pT8PPCwTULbHr2gnpRJMkrSMTJXx7M5LEsUGcNrpu","public_key":"TST8CDA1QNm1Fmgc4pDGHwBfWC17tHhvjg7qe46Wj9F5xGXv1dFWm","account_name":"witness0-eu"},
          {"private_key":"5JAASEGVck59nbV9qr9zXDxnxXCLp2YDMSVDJCfU7YBdZZh2RJ6","public_key":"TST5AufSRV6qLVu3KbN7nqUwgiAzBF9yumDXQH8dgT218qGn17w9x","account_name":"witness1-eu"},
          {"private_key":"5JzDgYfuW5uZkRF7xnJ8LbTjwcPzY7exGB7uo4m3Ns1eoFnF3Gj","public_key":"TST55MWVG9rhjr5bPUygTqiaH9UQfskdwW7BTZbsW7drxmeGBuDRU","account_name":"witness2-eu"},
          {"private_key":"5JzWBRwm4N7kE2uofcKoyNLCLtarAhjipRaoKLSWoPmykzTSVCi","public_key":"TST8St56XguN3WTWxumG3b8Zyi5shkgbqtpUPfYBiwciAqntRqWov","account_name":"witness3-eu"},
          {"private_key":"5JdDuyLyf6B8g8iC83DbgpCmpNe2QKnvGX8W95S3T36q6nXgg1E","public_key":"TST5gtJwYnc2CgNPsjYcHSTg5RNFURHpMiSnBnirq9XDxX2SSawX4","account_name":"witness4-eu"},
          {"private_key":"5KEEoUPDGRNZwfEpa6gMTGBWefqyR5t43mZoomo2q8vBJfZaWKp","public_key":"TST73Dc4RLLWv6gXVnkPSptJoriLzRACivnYZQ3BYV1edctGVS9hz","account_name":"witness5-eu"},
          {"private_key":"5JwCs2SSWUxuQbizjX8tzG6fSZ7CFxwfX3cD6FnpAnwnMrUXWkj","public_key":"TST7Z5gZakWdDhhNsHTsnX2HxRRkV5MjC7upz8DitNcp8tkJZUiAW","account_name":"witness6-eu"},
          {"private_key":"5KNWrP1tvQgqEhraLsNFBx8vq4Rti2iLSrFuRt1eqM4wRc4uBw5","public_key":"TST7LMqcEat7FzKQtJCCJQR34G96Pr6UL5G8YamSjVDecjwafrrZx","account_name":"witness7-eu"},
          {"private_key":"5Jb3cfsDv2LzUJEF9QJQWPDLuETVfbGwYcGpm8pLWTnPPBBUgtH","public_key":"TST7u2pnVKAiLZrFFrE5bfZ9Cid6xyfT7k3HYBqzNUFi4YioaA6ix","account_name":"witness8-eu"},
          {"private_key":"5K888UcxL6LJLmR2Gt8VT2VwKAVF8UAnPjgkBSYuup4tjr2PL5N","public_key":"TST6PAmErYvZ9PomEsmNa8Txy2tSqqQrwNWRDq2vbf6XqYou6JmU2","account_name":"witness9-eu"},
          {"private_key":"5KZDEVSDmtHFouchQyPjUonPuVRp4WXEwMGEe22ACkwaZAc4cUZ","public_key":"TST79aFGVMcQUWSeKyUh5V8m9H8sKzQzCwSzQNrwkNxhYgoFssKUw","account_name":"witness10-eu"},
          {"private_key":"5KY5MxvEMwhuBYSFQzfExHw2VtGzEuMbs3f4ZsZA8LKpXsDFQ6r","public_key":"TST8ea6N96QoEBG64bAMdat5UAR7FLbkY7Auyt7GZ5ZoBAo1mFoti","account_name":"witness11-eu"},
          {"private_key":"5KK687QaMfG8UujsE8CdzUEEmtEJcTJQqWsyt1M5ZXAGUDXY8EE","public_key":"TST8JydfSEY891K1VGed4e83oBbVpNtU4DBZULyRcCkkqjpCeFehi","account_name":"witness12-eu"},
          {"private_key":"5JfSNwXDSXFWbCvjDAHQkfGUZe8UXbJtqkqASGKN3EvpT76dnrc","public_key":"TST7adQHXRLAMyEVs1VR9nGpGhmiUdJ5EMqxU55maVNzor4Ady181","account_name":"witness13-eu"},
          {"private_key":"5Jatmmf5WnoM7TQqaVvdFU2uEcsYXjYd11pxfbhNrVuYJgyN8R2","public_key":"TST7M3gMQJ4HTGvPqUZEsykXKYnhEQ8ViGd9sUEBEDxV1GtgqdRZA","account_name":"witness14-eu"},
          {"private_key":"5KkwicAeJioGf48ayCySa5SKQnpXAVdp7PxGotQC5TFc6Vyq6nA","public_key":"TST7pUd2bHDVpS7VX1v7Q21tSu5c81Sy5mGFYALSdqX7qJECjWuUR","account_name":"witness15-eu"},
          {"private_key":"5JeqEP9F13McUQRGWVF9fGaCDjTGa26aVtW2sqBhGkd2yh6985N","public_key":"TST8RjLdeswyoUs5c2gCfVd4Mir5QsHYx2xpJtwLqYfgWtFgmWaUf","account_name":"witness16-eu"},
          {"private_key":"5KA1zZjti7u3s3vwmtYFsguNGYcAozu9XdEiEp3wdbzQtompZ9D","public_key":"TST55XTSb4WJcELGS2xpCLHccHdiuYJrtK3dSRc7hKXruyQvofsW5","account_name":"witness17-eu"},
          {"private_key":"5JENh2i3wddrfmQLGDPBgRcs6ExppCHVxj9ZJcKTQ12Aknnxc72","public_key":"TST8TTvCXuoDnpw1Tv6989JZtMCSh3TrAoKosniqwE65eMzL7e1q1","account_name":"witness18-eu"},
          {"private_key":"5JFBU3N8u1vMVCXzCQRixqC7cp3xT9iJvVm4v3iHYjAdJg53hVn","public_key":"TST7GYPzCkWGVqUF2i9PvpyqFQRPAAKMHVQtyLMZwDbLrE6gwjUGX","account_name":"witness19-eu"},
          {"private_key":"5KZ7K7jsYw3zYdVZJrdczgeLY8S494esHMGY3HuqWZdnswMP4AB","public_key":"TST5M1BqdwHX8tD4huJAjzyC5y81iPFy5ZDGnoEi7vZxGBuXVHBnE","account_name":"witness20-eu"},

          {"private_key":"5JEKR6uuRjjsfVQuNkH3fs139bXvmNj4YF5MKV56SWQ2398HVCb","public_key":"TST6nqBT2NbZpTwkKo6K6PxhRgUb1B7m8aQ7zjmzEEKXso2uaLdGC","account_name":"witness0-us"},
          {"private_key":"5KDA55TEiAJPndbSHKdtL3pHGcBCj7cDKCeZ6xmpEssSakU8m1o","public_key":"TST8PKQpAD4RAfmooBAQyNESUVi1fbh4uQKVViuYnZgk1RtdF1UfR","account_name":"witness1-us"},
          {"private_key":"5HsEPFYxsn27e7CnotcAWBBuKtjUJXd5roVVJr5StcRNJu9NCTA","public_key":"TST8azx5PjS28inVPoiUNG3H5zsAgSRhW8adiVBgeJ3cTTQ1nRArY","account_name":"witness2-us"},
          {"private_key":"5KPvwubAF36SiipAW66JwCwoNzjzNKQbC7KaVerEwZNtTozv7ZV","public_key":"TST6cmFPzbSRcyAFFS9uPqPV3kFJQiQyWxrgSqNek8qBvq6XrzkxN","account_name":"witness3-us"},
          {"private_key":"5KjnVeMk19xZCecmZXU9XsNXj3uikchLkzYZiAQuFsu1rVJqccu","public_key":"TST7iNkPe8RYVD1CRaW5p5MSmHNuWC6q9BimYLXwmckYWqE4uojPL","account_name":"witness4-us"},
          {"private_key":"5Kc3h6pTUg64gc7uB5J1jjSSHt2ACxHsQZFCjbBoE9qcVVfeCbC","public_key":"TST6iquFxmiY3qRsyL9fBtrRCGVR7nXg2F6ifATyvWxaKrJQN3NmZ","account_name":"witness5-us"},
          {"private_key":"5KYUdUHXv3CFw1HXQ3fiKYrBu6pCfkrQpBDcmTfmndzSMpR3rPe","public_key":"TST7jxNiXAX2HcjEBjgcaSrAgF2Dqr8iNtYAXJEzn11oM7cP2p2Yn","account_name":"witness6-us"},
          {"private_key":"5JVpVh4FTZtgpR5e4weDbCkFAYH3EuorLcDkTue2miPuQ8vqntn","public_key":"TST7rwRUAuX512t3DychHMeCBb9SP6zmBqdK8Hwv24ZL2UwxWUGHg","account_name":"witness7-us"},
          {"private_key":"5JyTbRuTFuZYx4omUu81fdbbfgf7C1eLh8jXH1iKdhhM8Zn9StQ","public_key":"TST67STHCvLHSyCCqPtZtzPdoQtvKxoqj9X78FY4LcbifEW34DRYy","account_name":"witness8-us"},
          {"private_key":"5KKwwni1VaQQodMaLPpUFWsQT8saSTrYY3zhR7mEhEoiCWBNDUL","public_key":"TST8gzGghQ9jwnuHPL8bfXW2vJSDWNhZiwjjHCui8mMj98jhfqasF","account_name":"witness9-us"},
          {"private_key":"5K4tP9TayigHA7msLanbDYQgG6DkxqJUxmeEgGGY9MEekJdy3rD","public_key":"TST5vokzkDbGbmb4SjjuSDVLgTj3SXLU2BzGxkQKksGKpEi311roq","account_name":"witness10-us"},
          {"private_key":"5JVb1eHta27P5JDF52Gkzf7GogxPRYeHuRRD3rqvL199LkbYhR5","public_key":"TST8iB8epvvTWvZyFz2iyni2LV1XLMaLMsUCS6HJyCXXmPoqSEBwJ","account_name":"witness11-us"},
          {"private_key":"5JkiX9ckcBzLya22BP46D539sKZsVGBBoZZomZicS9bmimpwE4V","public_key":"TST8Brk1zquQ55aXtuQq4rDyjtE7Nq4osMFdkTEKukzQqZKgAi3tt","account_name":"witness12-us"},
          {"private_key":"5KC1bv25NPWDi2pRwZ4CHmWzt3bDpYseSZ4nrn7Wh1AqUbNGWvb","public_key":"TST5qzPbQgRviP59HKrrwbBhnKj7i3DKuGWCxJDG82AHPgYrVsbni","account_name":"witness13-us"},
          {"private_key":"5KfT2jUE4XxH1pFekgQBXFrQ9EvBLpiWv3GcWj4nRw7eZcxY4XC","public_key":"TST6KHsdmVKNQJtNSHexwbbQwvD5ghULJ2s5vTetbShSxHNHHU2gN","account_name":"witness14-us"},
          {"private_key":"5JYMWGqk2FE2oQmRMRYZ2ff1DBrvm3cFnNneGduL5yASJ26bkUu","public_key":"TST8g84XvhLuRvTpyY7ThnBUxDc1m5SB4HssJEaBTtmyirNEruPw5","account_name":"witness15-us"},
          {"private_key":"5KbZnJuizkg1F6ZQRUPkXgJik9RQn11z4Cvr6kgLb9DnPj7VB9f","public_key":"TST7wxuxvb7BJPcdabWZwFEoLhf2pXcJnAkT19urv7yHH2sPc2bGG","account_name":"witness16-us"},
          {"private_key":"5JDzrp814zfGrjaWcSzTvwJDWSDTySLVUyM2Q5cYU6VYsdYEYV9","public_key":"TST6kQLyikDkdakroHVtHFQdxoddVt5nWE5SwgYj6KPJ4ai5JutiR","account_name":"witness17-us"},
          {"private_key":"5Hse3LAQNocDJ543yfWEpzBgNDUz55jF7FNbupR7euesNqBwxS9","public_key":"TST5GJFPrTvXhuL2nBHUc9GsqfzLyvDYGBUw5WPvFdrg6JgDTRFr9","account_name":"witness18-us"},
          {"private_key":"5KfAjejWgrVTex6HweRprpB1F3Fj7Ljn79Y8Ng5sMAe51a1TfyY","public_key":"TST6VJZ5AV7fgZgyWGQsKWiw8pALhPMHUGHgR3pGxzsviphsqd2qQ","account_name":"witness19-us"},
          {"private_key":"5HuKhXPbEkZidHMfi9gKXe34rfo914xu3xAPJk3N9MMUu2qzjwu","public_key":"TST73c5GWVMsCdtsbMdexe8xXf5JYtpCGwFb1PMGjaT5BpsxNqxAd","account_name":"witness20-us"},
        ]

        random.shuffle(witnesses)


        wallet_eu_url = "http://localhost:3904"
        api_node_eu_url = "http://localhost:3902"
        wallet_us_url = "http://localhost:4904"
        api_node_us_url = "http://localhost:4902"
        mainNetUrl = "https://api.hive.blog"

        #initminer key
        set_password(wallet_eu_url)
        unlock(wallet_eu_url)

        import_key(wallet_eu_url, "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n")
        error = False

        print_top_witnesses(witnesses, api_node_eu_url)
        if True:
            list_accounts(wallet_eu_url)

            prepare_accounts(witnesses, wallet_eu_url)

            print("Witness state before transfer")
            print_top_witnesses( witnesses, api_node_eu_url)

            configure_initial_vesting(witnesses, "1000000.000 TESTS", wallet_eu_url)

            prepare_witnesses(witnesses, wallet_eu_url)

            print("Witness state before self vote")
            print_top_witnesses(witnesses, api_node_eu_url)

            self_vote(witnesses, wallet_eu_url)

            print("Witness state after voting")
            print_top_witnesses(witnesses, api_node_eu_url)

        list_accounts(wallet_eu_url)


        node0_eu = {"url" : "127.0.0.1:3002"}
        node1_eu = {"url" : "127.0.0.1:3102"}
        node2_eu = {"url" : "127.0.0.1:3202"}
        node3_eu = {"url" : "127.0.0.1:3302"}
        node_api_eu = {"url" : "127.0.0.1:3902"}
        node0_us = {"url" : "127.0.0.1:4002"}
        node1_us = {"url" : "127.0.0.1:4102"}
        node2_us = {"url" : "127.0.0.1:4202"}
        node_api_us = {"url" : "127.0.0.1:4902"}
        nodes_eu = [node0_eu, node1_eu, node2_eu, node3_eu, node_api_eu]
        nodes_us = [node0_us, node1_us, node2_us, node_api_us]
        nodes = nodes_eu + nodes_us

        for node in nodes:
            node_id = get_node_id(node["url"])
            node["node_id"] = node_id
            log.info(str(node))
        
        node_id_eu = [node["node_id"] for node in nodes_eu]
        node_id_us = [node["node_id"] for node in nodes_us]

        for node in nodes_eu:
            set_allowed_peers(node["url"], node_id_eu)

        for node in nodes_us:
            set_allowed_peers(node["url"], node_id_us)




    except Exception as _ex:
        log.exception(str(_ex))
        error = True
    finally:
        if error:
            log.error("TEST `{0}` failed".format(__file__))
            exit(1)
        else:
            log.info("TEST `{0}` passed".format(__file__))
            exit(0)

