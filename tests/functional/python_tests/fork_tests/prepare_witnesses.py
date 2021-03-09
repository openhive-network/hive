#!/usr/bin/python3

import time

import sys
import json

from jsonsocket import steemd_call
from hive.steem.client import SteemClient

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

    status, response = steemd_call(_url, data=request)
    return status, response

def unlock(_url):
    log.info("Call to unlock")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "unlock",
      "params": ["testpassword"]
      } ), "utf-8" ) + b"\r\n"

    status, response = steemd_call(_url, data=request)
    return status, response

def import_key(_url, k):
    log.info("Call to import_key")
    request = bytes( json.dumps( {
      "jsonrpc": "2.0",
      "id": 0,
      "method": "import_key",
      "params": [k]
      } ), "utf-8" ) + b"\r\n"

    status, response = steemd_call(_url, data=request)
    return status, response

def checked_steemd_call(_url, data):
  status, response = steemd_call(_url, data)
  if status == False or response is None or "result" not in response:
    log.error("Request failed: {0} with response {1}".format(str(data), str(response)))
    raise Exception("Broken response for request {0}: {1}".format(str(data), str(response)))

  return status, response

def wallet_call(_url, data):
  unlock(_url)
  status, response = checked_steemd_call(_url, data)

  return status, response

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

    status, response = checked_steemd_call(_url, data=request)
    return response["result"]["witnesses"]

def print_top_witnesses(sockpuppets, witnesses, api_node_url):
  sockpuppets_set = set()
  for w in sockpuppets:
    sockpuppets_set.add(w["account_name"])

  witnesses_set = set()
  for w in witnesses:
    witnesses_set.add(w["account_name"])

  top_witnesses = list_top_witnesses(api_node_url)
  position = 1
  for w in top_witnesses:
    owner = w["owner"]
    group = "U"
    if(owner in sockpuppets_set):
      group = "C"
    elif (owner in witnesses_set):
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
        init_logger(__file__)

        sockpuppets = [
          {"private_key":"5HsBnrPbtqGki6E4mmgT5QevVotAFN2f1TafB7yjfQu8eBTtkQ5","public_key":"TST6NtyPP4Ar1rZvGyS4bmdwjCMpQvCoF3TQ3TBjgfBCH7q982UDr","account_name":"sockpuppet0"},
          {"private_key":"5KUSsSXQiVRw9z5ySk6hHfmHVcH2Q24qo9iDhYRLmnsab5vpbQq","public_key":"TST6XS6Bhr2PuVRa9fDpAS2fona6n3Uw78qqKqhbVsW2y78Fisuuu","account_name":"sockpuppet1"},
          {"private_key":"5J71Rjrme5pHZG6V6pbNTgiiV58e3nHygjUUDmEsWEaNWnrXRK9","public_key":"TST8m9GRyzA8YyCVSJBhg96LUFo395WrJYW3imkgyR9bLtjLNKJa8","account_name":"sockpuppet2"},
          {"private_key":"5KSvusFLKw9sZXudLmJGzAnobQHtbAKVdSiPTpCT13m14W6vyZL","public_key":"TST5ERgKCet1kRioxhKHpCES5uakFU53Sz2QsihqS8T7zGarc9Us8","account_name":"sockpuppet3"},
          {"private_key":"5JY9njeGFXVpqwoWG6MKEYPrn2iRiG3ac4iCqavPzTGjxgxinHt","public_key":"TST7hqbYGsHz4CgDhXRrt4RZ2GVqVrZyEwzSjRNQQFSycdn3YR8hB","account_name":"sockpuppet4"},
          {"private_key":"5JZynGSSWmCy3NbMknV5xDMLwKiMFqXZb5NpAbY2crikqJboYiB","public_key":"TST5SJvzgw7EugmhLW1meCRzj9astkCngjd2TBZjfbfywQDfvM1aR","account_name":"sockpuppet5"},
          {"private_key":"5KbGSDfz1bMBmBVQEdCfHCBQ9PtM8mrzFhJpwuWvRcVv5rFNLTZ","public_key":"TST57S4UhWbPEZE4xRD8szWDYYquo1u6GCcRo9vR3STCukMcBQn8t","account_name":"sockpuppet6"},
          {"private_key":"5HrEfVKqBfmx2bhywXnfrLG9hapGgkLkD7H4AtDn9UoCQzX9rkW","public_key":"TST8d89ybxZw3pWcG1YSoQSG8yRJj8xxykeVTpYc4X5PKf4XDsoNv","account_name":"sockpuppet7"},
          {"private_key":"5K7VanJNgUPhqS6keGh7nJPMy8aispTahZt2A7kwyQt9c1XVVCk","public_key":"TST8hqEhPGNXnSMLcEkNh8VpKUAN8jHWx6QsYVhDiBt9FNCmCKuVw","account_name":"sockpuppet8"},
          {"private_key":"5Huk5GiW9kjNG1UhnT2vQSPAMPtSfrESz8EgBE3WvooMFCDcqDU","public_key":"TST58VFgi43hU92DESuQvbXxCh1R3XhNupU5RXnM1bPZ1yyBzhWC1","account_name":"sockpuppet9"},
          {"private_key":"5JEazsggtz2y9AZErw7rT2Bzi6FJxDyEtWZZTiHVFGd2C8zcJiD","public_key":"TST6amW3bHfyDNFgREmcwUqB3REcN9X7mdB1PLPPmcUAc1Ew8DJdX","account_name":"sockpuppet10"},
          {"private_key":"5K4ijr5aqtTMGBgoQyRpCMLn7kS2XNqsNAU2V1reNfPe7iEe6BZ","public_key":"TST7VyjUAZvNVNKXkYSJyNR5H12LSiRvRVXL6wQSw9uGgMhJf7fyS","account_name":"sockpuppet11"},
          {"private_key":"5JJCo62Ahf98eiJgCCxposX7C77nqwfUnEPMsCU8Ub8jkUBY39W","public_key":"TST87jZaB6bVgdch2UbYanGJ7vBEajLdfvtVvAqHxV1SwjQbhYEXC","account_name":"sockpuppet12"},
          {"private_key":"5KQb7Pm5BuAMnpYi11G9QHYn6pmcyHxm3183h7ZVntP2Y1qAiRF","public_key":"TST51boQ21dCdAY4PwkdKLYiLpQLtKg6QRqLjJdfU4Maz35XkBwHP","account_name":"sockpuppet13"},
          {"private_key":"5JMFfpZGUuwFWU9PEMQ4wGuvzXddrsGFGwcWP9QX92DrU9RnozH","public_key":"TST6yGkrQbL5hMoPqzdY3B3BH3h8uXqkQxwmz3bor5q6bW3B5LBWS","account_name":"sockpuppet14"},
          {"private_key":"5JHpzjSHmU81GFtVqzAJwtH7iqbRQGVN9ZafFPJdhyfWZVZmYFB","public_key":"TST7U2Kik7ByT3zNCLFuFEDKorGMau2tsRAgBnMdgrntm2FnBGBc2","account_name":"sockpuppet15"},
          {"private_key":"5KVV5ieddNmLVtCYh7eR8TTRdjeA6ppAi4smfApuaMJdZ9St4XR","public_key":"TST66Xo9kV6WYJSoMp9yWJV3hde8PJrbKHmvhArEuHnB5V3xwXcs8","account_name":"sockpuppet16"},
          {"private_key":"5J6x6jh59jJetpAA8rh4XiDzLTSch1nSuA7Euj4hnvjn4JpwFcq","public_key":"TST7K9td5ssg1SqqYKyZKPmBj4tekjq35iehkskV5ixaAc8EZy2n2","account_name":"sockpuppet17"},
          {"private_key":"5JRAHYir79jhiYtTX8F4T1tzvcwvth9fmGsPJdS4rXrpVpCL9L6","public_key":"TST6nn9EVgyhfaGy1Ve59MjjaWD6kPntKC26hsMzo1WgdQ7R2eqm4","account_name":"sockpuppet18"},
          {"private_key":"5K2gJh9ctyjwMhcUBJcWCxFHeKQ2GBwPHYg6SKTSBYi1qhhn8JJ","public_key":"TST7hN2UWcarAnzangXGrRD7jjzagUBR1gzmD3eBNAbdKSHp3ASJu","account_name":"sockpuppet19"},
        ]

        witnesses = [
          {"private_key":"5J3eWK6nveqtjd8z6Gk9zLfcJCfGf6jgteouGZL8q9oRD89Fs6e","public_key":"TST6xEvC8yYLLpeC9wE5iNKWTweXgTJufLegXYvDyPhE3GfSPGDor","account_name":"witness0"},
          {"private_key":"5Kji6CXkau7xm1cegSVHrL9dfH6gXC3FfN5NxRqA2KevQGziKn8","public_key":"TST7j81uJfnUDHr7BxSc8Guri36B2SUgEhasJDV96U14LQ9BKXAMF","account_name":"witness1"},
          {"private_key":"5KfGxXsrmsJh2YUjYc1AzpeaunCFMn4UJLMuaZwEowcBcCKdPgz","public_key":"TST6x2KFKq7GjMX9XoJ44AtSn7oPjUqrt89f7AuAGs4kjpZNn5UJN","account_name":"witness2"},
          {"private_key":"5Jp3Nch1Sq318eWuYeKg5jyTh2mc7LPjuRCR7Ktd7xUdHuu8N15","public_key":"TST8gzMCG3gQFePJcBTcry3j7iXk3ayXuKUajwiaugWr6hQPopEwM","account_name":"witness3"},
          {"private_key":"5JLk8YmfegVXMESrqqfF2AbrUyQqDjh7BVTR5frB5wJtTFqmZn3","public_key":"TST8GwL3EDz7Q1CwJ79wi8ZeEBweLcHWvYexst6ZsftGGUDFSPBJJ","account_name":"witness4"},
          {"private_key":"5JztghB976dHJUYqxVpyCwCTQG7wYE4JeZSVWNF34XpgQHCRtSf","public_key":"TST6kM9GZF9m42S4K6LL8nS3QtFi1D22gvHGVJJW1koxFsMhKp1xP","account_name":"witness5"},
          {"private_key":"5Ju59s9AQcWohm6RJYW3EwyWYGowhniJNYQqNiHcy8eh5MC171z","public_key":"TST6vsEAFvaEv6oKN3nLnwSwJnssTxq9evrMkj1YoXrPwHabxpxJ1","account_name":"witness6"},
          {"private_key":"5KHBiEGvAuZ5oasb66t4aW5uP2NUyrFJa4QJQE1wxTbHJQmVNBN","public_key":"TST8LGfK8X8m49ApyG2cnQ2M9k96PCVaXDnkJDt3sXYc9qCTpCiVr","account_name":"witness7"},
          {"private_key":"5KEaGL3pxrNWQChnsGKJQTVVpGBpUw5q3ECft71MChhVL1PheTu","public_key":"TST8Ga8wQ3DgFB42EF9Z4zSJHBRcCgnA52myW1nMurJbJufGoLm7h","account_name":"witness8"},

          {"private_key":"5J9C3EFpRP9FbV8sQ6NUMgniHfsuQ6m4sb7oBP8a2JvjmjPPGg4","public_key":"TST6SnTJH55Vk8JqneW3hf8noL1s4sKPwuv6TUEuwm32pGccquo7c","account_name":"good-wtns-0"},
          {"private_key":"5JiWo1BEsB5ojmXfspCxzrxMqatNf8F3Y5ZMGWGkHHkCx6t6eZj","public_key":"TST5ehaXY5DfEgsLoR5pa9oLAgh69FPrhTgiohQbPhfHhXgtyBvab","account_name":"good-wtns-1"},
          {"private_key":"5J9rbEcvS4pLNE5RtvAV9QQ7XRq5kbjaVyMYKn8srtfWdKdVHN9","public_key":"TST6WQtvF1JvYaEVCjLpLX3sHcoNgVj2hsrohyqKh1QM4yLg2c8t2","account_name":"good-wtns-2"},
          {"private_key":"5K9DFbLYfdCdFMhUuytzC6xR9TCRZHJPCopC1HNJAzCoPSs8QXR","public_key":"TST5CrhzfmZ4FuXyu6UUTDWJgcqSx7SSCzyouafqTxXbDFtuM1uCD","account_name":"good-wtns-3"},
          {"private_key":"5KX2vnJfNGTNVpCvdwWyPtavyLw57mycwvWpT2zD4W9pMVqybGu","public_key":"TST5oyQZgDQGeqiTzoLhxLWg7eVSe47DZhxCDY7GxBDH1DjK89rAn","account_name":"good-wtns-4"},
          {"private_key":"5KYoUD7jKbJs5hYYfnEfeEp66rYoesDwLLq5rkYpQjQvLWjuAE2","public_key":"TST59agdvEHTp7Pjj3Uy9GBsjKYRqLSsdLZtk2wHwRs7EtePCxs35","account_name":"good-wtns-5"},
          {"private_key":"5JjiZR9xoe8jAauotYY48g6tQKaVqUJbAZKSv8Wj2mYQLQfZ9SP","public_key":"TST5BpLtfnuF87HhJWXkpWTVsyN2fDMbvtiGzvctM1oADvzf6EKBe","account_name":"good-wtns-6"},
          {"private_key":"5JSXDCKzo2yjgy3qkRUHekPYRXTfidDGDpMaYDVaesYgMHPs3Tp","public_key":"TST8bLzBR5xxTxBrpUtZbSm69SX8HY8xLfnVPZFiYLQA1JW9a2Nh9","account_name":"good-wtns-7"},
          {"private_key":"5HrCpetwxVtKPrdmRmDsBh4QT45bdF3LoLCVsrPg1v3tS4nXmps","public_key":"TST6gpjNQfja64G5ESGPZrecwisw2TpVzkRNfQFNAsECzTqszw4HE","account_name":"good-wtns-8"},
          {"private_key":"5JPi9C6yKKiZWzaz5Py7a2zNp1mbFkZNQ51y1D7R7EJ7AtffdEZ","public_key":"TST5VEgRupNeeudQxxThPriS97infAPaDNhP6uB3R1fZnUutLyRKm","account_name":"good-wtns-9"},
          {"private_key":"5KSL5CAA4XWzuLpA6LXb9oDdFCjsMGLpvgW1Uawk4vP2UmBu4GL","public_key":"TST8YxodyhdChw1Xhhmg15dUpvSvNsDnzYZh5Sdi6kpqri7GzN4fJ","account_name":"good-wtns-10"},
          {"private_key":"5J7bVoA2GJL21zfJbqgHFmkZ1ppnKmGR9Deg5vizhcfozmnrCDQ","public_key":"TST6WZN5kdd6DwHwJfHbz88qEJRVmwrx8RfH1uFAcqieEjbBX1SBu","account_name":"good-wtns-11"},
          {"private_key":"5J3AE5kNeq7kk1KUZrCwDuB1VfZuNasPjmuxqkoTSwvW3qKR17V","public_key":"TST8KoMLz8WjVznJVhEpzMEjmWWZ94BDYiMDJRj35VC9hwWqpcqBR","account_name":"good-wtns-12"},
          {"private_key":"5KQRiTFkUmHYLsM56VRFiDZiGX6sMWd55WcSYnRbehhG5yjcYFW","public_key":"TST5wHR6mWVwT68FL8m5KUDS91HbmkrzNXCAeYVY4CdTZXB8sZgr2","account_name":"good-wtns-13"},
        ]

        steemit_accounts = [
          {"private_key":"5JkToXsx5jvf9MBuG9iNusBypU3jYGYAi7PaUDFTGqqV2wvVtBe","public_key":"TST4vSpS2spVbwimhCYr3Xbexus8QaNHWQjzgcZ2qfzpSfNZ64FAr","account_name":"steemit-acc-0"},
          {"private_key":"5JpjCir3qszJTsU6gpHGaH8CkPrqrvEqdup6nHXaeWies8nQg5L","public_key":"TST5yL7XLyF9iNGsrsSiGda7VtfEn77riGZFFuEJEu9uPKtF5syqB","account_name":"steemit-acc-1"},
          {"private_key":"5JckFAaVUNVpoMFmD3EkKEJkCrm1LMPFY2iTAkyYzQWmfU2FSp6","public_key":"TST83Pkj4VKuu85VpPp7FXkijmf6N5wqRVTz8CBWHM2yg54kWnLJH","account_name":"steemit-acc-2"},
          {"private_key":"5KJZhfSEtnW5Kb8hdBAvpAkTD4Qk1LC1iAUkoXFdQfown4Yp4Lo","public_key":"TST5cCeaDb2uNczHAjxJPegrZoQR96BuMTMPiMW4mg9ukTiGwE2Ev","account_name":"steemit-acc-3"},
          {"private_key":"5JPKa55o515smdNfKexNB1tYEGdhYN41c6tJaSSnNYAcKEeSedj","public_key":"TST4uqws5V8LS2nPcCFYJez6iZUSU1xYQhWPjt8XTkwJw9cXKfoyV","account_name":"steemit-acc-4"},
          {"private_key":"5JMQcBenSXRaAAtjfUzmj7Q1V9CjnkjVETjcFhSQWLPGSDYkPL3","public_key":"TST8BKkjji8PoAhLZhgS5K8Tt4CYiuNtzC9EKMg5t84e31kvnsBoD","account_name":"steemit-acc-5"},
          {"private_key":"5JATcexwiWooZu2XdXRhyxkzK4uAjsxwcQC2hYBUCtEiQXtzHQZ","public_key":"TST61PgkDSm1XXRX2DAaPw7G7pgE9RRbeJbw2NxvfFL9MgUPvqN3X","account_name":"steemit-acc-6"},
          {"private_key":"5Kh4ityJkGFBLbNWf7meQ92tA53TctcELLL1AweoAJvL5GxVjiB","public_key":"TST5iVNNnogXJuwGWwB1KdxWpeuXXcvqvoUTbipamhMnUjCCvdjX6","account_name":"steemit-acc-7"},
          {"private_key":"5KUNiSvMPfHgwRCmpKfbW7o7NRgwabNrZ7Nw73nUnyqzhDD9U48","public_key":"TST7sDn7YwyiRvc8v6zirXoUSBE4EfWZYzq36dS52BkBsRdhBzGp7","account_name":"steemit-acc-8"},
          {"private_key":"5JtEUibG7b8qsDrVBa3Ff7My4KCPFxzhThWMadecbR8yjYKG5iM","public_key":"TST7G1qgxZXWK71fAUG4LTq36AvokJ62Mc3RXcbJ8QCDxf7G3FJoL","account_name":"steemit-acc-9"},
          {"private_key":"5KNX66DM5cT5FSGH6hgoLCYUTzAHU4uTi4oFDzEAMCRKRSyUQAL","public_key":"TST5oTJLwwKFzhkvTem2zUSaCBdqgM1eFNWFNKXevr626bb5q7adR","account_name":"steemit-acc-10"},
          {"private_key":"5JR7AFQHBSrK7inrEh8tz9pATjSK1n3363NzUXTUu9bLgh6bZfH","public_key":"TST7DgtzKyPr7nWXqfm7S3v1L6uMqF6unnX87uM9okp1koCHVg4ve","account_name":"steemit-acc-11"},
          {"private_key":"5JhaM84YJi7DaiGjNf7KadsTcwTTaNzAE8aW9Mb4wG4xjpFy3S4","public_key":"TST8crb47h8PvcjEdsdMvpUcJijozmLARKYetfVc66xDTfnRpkkQx","account_name":"steemit-acc-12"},
          ]

        steemit_proxies = [
          {"private_key":"5Jsc8KczJrK537MEgPDbA28MMD9fFFR5DQq77sM9qM8kuTJz9sP","public_key":"TST6SLYja77Lpz4tUrrSjEGWLbeuNtX5Rzymx8oBcpfgXyqWJs1yB","account_name":"steemit-proxy"},
        ]

        url = "http://localhost:8093"
        api_node_url = "http://localhost:58091"
#        api_node_url = "http://localhost:8091"
        mainNetUrl = "https://api.hive.blog"

        #initminer key
        set_password(url)
        unlock(url)

        import_key(url, "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n")

        print_top_witnesses(sockpuppets, witnesses, api_node_url)

        error = False
        list_accounts(url)

        prepare_accounts(steemit_accounts, url)
        prepare_accounts(steemit_proxies, url)

        prepare_accounts(sockpuppets, url)
        prepare_accounts(witnesses, url)

        configure_initial_vesting(steemit_accounts, "1000000.000 TESTS", url)
        configure_initial_vesting(["witness" + str(i) for i in range(9)], "12000000.000 TESTS", url)

        configure_initial_vesting(steemit_proxies, "10.000 TESTS", url)
        configure_initial_vesting(sockpuppets, "10.000 TESTS", url)
        configure_initial_vesting(witnesses, "1000000.000 TESTS", url)

        prepare_witnesses(sockpuppets, url)
        prepare_witnesses(witnesses, url)

        self_vote(witnesses, url)

        print("Witness state before voting of steemit-proxy proxy")
        print_top_witnesses(sockpuppets, witnesses, api_node_url)

        vote_for_witnesses("steemit-proxy", sockpuppets, 1, url)

        print("Witness state after voting of steemit-proxy proxy")
        print_top_witnesses(sockpuppets, witnesses, api_node_url)

        list_accounts(url)

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

