#!/usr/bin/python3

import os
import argparse
from shutil import copy2
import re

def log_text(txt):
  if log:
    print(txt)

def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument('--tavern-dir', type=str)
  parser.add_argument('-log', action='store_true', default=False)
  return parser.parse_args()

def create_dir(dir):
  if os.path.isdir(dir) is False:
    os.mkdir(dir)
    log_text(f"created new dir: {dir}")

def read_test(name, old_method_dir):
  test_name = '.'.join([name, test_ext])
  test_path = os.path.join(old_method_dir, test_name)
  if os.path.isfile(test_path):
    with open(test_path, 'r') as f:
      test = f.read()
    return test, test_path
  else:
    return False, test_path

def copy_pattern(name, old_method_dir, new_method_dir):
  pat_name = '.'.join([name, pat_ext])
  pattern_path = os.path.join(old_method_dir, pat_name)
  if os.path.isfile(pattern_path):
    copy2(pattern_path, os.path.join(new_method_dir, pat_name))
    return True, pattern_path
  else:
    return False, pattern_path

def write_test(name, new_test, new_method_dir):
  test_name = '.'.join([name, test_ext])
  test_path = os.path.join(new_method_dir, test_name)
  with open(test_path, 'w') as f:
    f.write(new_test)

def change_url(test, method_name):
  old_url = re.search(regexp_dict["url"], test).group(1).strip()
  if old_url.endswith('/'): sep = ''
  else: sep = '/'
  
  new_url = sep.join([old_url, method_name])
  return test.replace(old_url, new_url)

def change_json(test):
  old_json = re.search(regexp_dict["json"], test).group(0)
  ident = re.search(regexp_dict["ident"], old_json).group(1)
  params = re.search(regexp_dict["params"], test).group(1)

  new_json_list = []
  for param in params.split(","):
    param = param.strip()
    param_name = re.search(regexp_dict["param_name"], param).group(1).strip()
    param_value = re.search(regexp_dict["param_value"], param).group(1).strip()

    param_str = f"_{param_name}: {param_value}"
    new_json_list.append(param_str)

  new_json = f"\n{ident}".join(new_json_list)
  return test.replace(old_json, new_json)

def create_new_test(name, old_method_dir, new_method_dir, method_name):
  old_test, old_test_path = read_test(name, old_method_dir)
  pattern, pattern_path = copy_pattern(name, old_method_dir, new_method_dir)
  if old_test is False:
    log_text(f"pattern exists, but test is missing: {old_test_path}")
    return False
  elif pattern is False:
    log_text(f"test exists, but pattern is missing: {pattern_path}")
    return False
  new_test = change_url(old_test, method_name)
  return change_json(new_test)

def create_hafah_api_tests():
  for test_type in ['negative', 'patterns']:
    old_test_dir = os.path.join(testsuite_dir, '_'.join([old_tests, test_type]))
    new_test_dir = os.path.join(testsuite_dir, '_'.join([new_tests, test_type]))
    create_dir(new_test_dir)
    for method_name in ['enum_virtual_ops', 'get_account_history', 'get_ops_in_block', 'get_transaction']:
      old_method_dir = os.path.join(old_test_dir, method_name)
      new_method_dir = os.path.join(new_test_dir, method_name)
      create_dir(new_method_dir)

      test_names = list(set([name.split('.')[0] for name in os.listdir(old_method_dir)]))
      for name in test_names:
          new_test = create_new_test(name, old_method_dir, new_method_dir, method_name)
          if new_test:
            write_test(name, new_test, new_method_dir)

    log_text(f"finished creating hafah api {test_type} tests:\n{new_test_dir}")

if __name__ == '__main__':
  args = parse_args()

  testsuite_dir = os.path.join(args.tavern_dir, 'tavern')
  log = args.log
  old_tests = 'account_history_api'
  new_tests = 'hafah_api'

  pat_ext = 'pat.json'
  test_ext = 'tavern.yaml'

  regexp_dict = {
    "url": r"url:\s?['\"]([^\"]+)",
    "json": r"jsonrpc:[\s\S]*?(?=\n.*?response)",
    "params": r"params:\s?{([^}]+)",
    "ident": r"( {2,})id",
    "param_name": r"['\"](.+)['\"]:",
    "param_value": r":\s?(.+)"
    }

  create_hafah_api_tests()