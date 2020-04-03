import logging
DEFAULT_LOG_FORMAT = '%(asctime)-15s - %(name)s - %(levelname)s - %(message)s'
DEFAULT_LOG_LEVEL = logging.INFO

def send_rpc_query(target_node : str, payload : dict) -> dict:
  from requests import post
  from json import dumps
  resp = post(target_node, data = dumps(payload))
  if resp.status_code != 200:
    print(resp.json())
    raise Exception("{} returned non 200 code".format(payload["method"]))
  return resp.json()

def get_random_id() -> str:
  from uuid import uuid4
  return str(uuid4())

def get_current_block_number(source_node) -> int:
  payload = {
    "jsonrpc" : "2.0",
    "id" : get_random_id(),
    "method" : "database_api.get_dynamic_global_properties", 
    "params" : {}
  }

  from requests import post
  from json import dumps, loads

  resp = post(source_node, data = dumps(payload))
  if resp.status_code != 200:
    return -1
  data = resp.json()["result"]
  return int(data["head_block_number"])

def wait_n_blocks(source_node : str, blocks : int, timeout : int = 60):
  from time import sleep
  starting_block = get_current_block_number(source_node)
  while starting_block == -1:
    starting_block = get_current_block_number(source_node)
    sleep(1)
  current_block = starting_block
  cntr = 0
  while current_block - starting_block < blocks and cntr < timeout:
    current_block = get_current_block_number(source_node)
    sleep(1)
    cntr += 1
  if cntr > timeout:
    raise Exception("Timeout in wait_n_blocks")

def debug_generate_blocks(target_node : str, debug_key : str, count : int) -> dict:
  if count < 0:
    raise ValueError("Count must be a positive non-zero number")
  payload = {
    "jsonrpc": "2.0",
    "id" : get_random_id(),
    "method": "debug_node_api.debug_generate_blocks",
    "params": {
      "debug_key": debug_key,
      "count": count,
      "skip": 0,
      "miss_blocks": 0,
      "edit_if_needed": True
    }
  }
  return send_rpc_query(target_node, payload)

def debug_generate_blocks_until(target_node : str, debug_key : str, timestamp : str, generate_sparsely : bool = True) -> dict:
  payload = {
    "jsonrpc": "2.0",
    "id" : get_random_id(),
    "method": "debug_node_api.debug_generate_blocks_until",
    "params": {
      "debug_key": debug_key,
      "head_block_time": timestamp,
      "generate_sparsely": generate_sparsely
    }
  }
  return send_rpc_query(target_node, payload)

def debug_set_hardfork(target_node : str, hardfork_id : int) -> dict:
  if hardfork_id < 0:
    raise ValueError( "hardfork_id cannot be negative" )
  payload = {
    "jsonrpc": "2.0",
    "id" : get_random_id(),
    "method": "debug_node_api.debug_set_hardfork",
    "params": {
      "hardfork_id": hardfork_id
    }
  }
  return send_rpc_query(target_node, payload)

def debug_has_hardfork(target_node : str, hardfork_id : int) -> dict:
  payload = {
    "jsonrpc": "2.0",
    "id" : get_random_id(),
    "method": "debug_node_api.debug_has_hardfork",
    "params": {
      "hardfork_id": hardfork_id
    }
  }
  return send_rpc_query(target_node, payload)

def debug_get_witness_schedule(target_node : str) -> dict:
  payload = {
    "jsonrpc": "2.0",
    "id" : get_random_id(),
    "method": "debug_node_api.debug_get_witness_schedule",
    "params": {}
  }
  return send_rpc_query(target_node, payload)

def debug_get_hardfork_property_object(target_node : str) -> dict:
  payload = {
    "jsonrpc": "2.0",
    "id" : get_random_id(),
    "method": "debug_node_api.debug_get_hardfork_property_object",
    "params": {}
  }
  return send_rpc_query(target_node, payload)
