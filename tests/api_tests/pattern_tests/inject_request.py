import os
import argparse

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

def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument('--project-root', type=str)
  return parser.parse_args()


def do_inject(target_file):
  with open(target_file, 'r') as f:
    target_data = f.read()

  if "custom_update_request" in target_data: return

  updated_data = target_data.replace(custom_func_pos_pat, f"{custom_func_pos_pat}\n\n{custom_func}")
  updated_data = updated_data.replace(custom_func_call_pat, f"{custom_func_call_pat}\n\n{custom_func_call}")

  with open(target_file, 'w') as f:
    f.write(updated_data)


def prep_inject():
  if os.getenv("IS_DIRECT_CALL_HAFAH", False) is False:
    return None
  project_root = parse_args().project_root
  target_file = os.path.join(project_root, ".tox", "tavern", "lib", "python3.8", "site-packages", "tavern", "_plugins", "rest", "request.py")
  return target_file


if __name__ == "__main__":
  target_file = prep_inject()
  if target_file is not None:
    do_inject(target_file)
