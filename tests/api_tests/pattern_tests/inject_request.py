"""
Injects custom_update_request() function into tavern 'request.py', usually found in
hive/.tox/tavern/lib/python3.8/site-packages/tavern/_plugins/rest/request.py
"""
import os


request_f_name = "request.py"
request_f_pat = "class RestRequest(BaseRequest)"

custom_func = """
def custom_update_request(request_args):
    if os.getenv("IS_DIRECT_CALL_HAFAH", False):
        hafah_method = os.getenv('PYTEST_CURRENT_TEST').split(os.path.sep)[2]
        request_args["url"] = f'{request_args["url"]}rpc/{hafah_method}'
        request_args["json"] = request_args["json"]["params"]
    return request_args"""
custom_func_pos_pat = "return request_args"

custom_func_call = "        request_args = custom_update_request(request_args)"
custom_func_call_pat = """
        request_args = get_request_args(rspec, test_block_config)
        update_from_ext(
            request_args,
            RestRequest.optional_in_file,
            test_block_config,
        )"""


def do_inject(target_file):
  with open(target_file, 'r') as f:
    target_data = f.read()

  if "custom_update_request" in target_data:
    return False

  updated_data = target_data.replace(custom_func_pos_pat, f"{custom_func_pos_pat}\n\n{custom_func}")
  updated_data = updated_data.replace(custom_func_call_pat, f"{custom_func_call_pat}\n\n{custom_func_call}")

  with open(target_file, 'w') as f:
    f.write(updated_data)
  return True

def find_request_file():
  if not os.getenv("IS_DIRECT_CALL_HAFAH", False):
    return None

  project_root = os.getenv("PROJECT_ROOT", os.getcwd())
  tox_tavern_dir = os.path.join(project_root, ".tox", "tavern")
  matches = [os.path.join(root, request_f_name) for root, _, files in os.walk(tox_tavern_dir) if request_f_name in files]
  for match in matches:
    with open(match, "r") as f:
      data = f.read()
    if request_f_pat in data:
      return match

  return False

if __name__ == "__main__":
  target_file = find_request_file()
  print(target_file)
  if not target_file:
    raise RuntimeWarning("'request.py' not found, tavern code not injected!")
  if target_file is not None:
    injected = do_inject(target_file)

  if not injected:
    raise RuntimeWarning("'custom_update_request()' already exists.\nThis should not be possible in gitlab CI!")
  else:
    print("inject_request.py: 'custom_update_request()' added to tavern's 'request.py'")