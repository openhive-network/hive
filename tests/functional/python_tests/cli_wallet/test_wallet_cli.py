#!/usr/bin/python3

from test_tools import paths_to_executables, logger

def test_help_option():
  from subprocess import PIPE, run as run_executable
  only_args_to_be_founded = [
    '--help', '--offline', '--server-rpc-endpoint', '--cert-authority',
    '--retry-server-connection', '--rpc-endpoint', '--rpc-tls-endpoint',
    '--rpc-tls-certificate', '--rpc-http-endpoint', '--unlock',
    '--daemon', '--rpc-http-allowip', '--wallet-file', '--chain-id'
  ]

  cli_wallet_path = paths_to_executables.get_path_of('cli_wallet')
  process = run_executable([cli_wallet_path, "--help"], stdout=PIPE, stderr=PIPE)
  stdout = process.stdout.decode('utf-8')
  args_founded = [ arg for arg in stdout.split() if "--" in arg ]
  diff = list(set(args_founded)^set(only_args_to_be_founded))
  logger.info(f'found: {diff}')
  assert len(diff) == 0
