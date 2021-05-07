#!/usr/bin/python3

RUN = True

def get_config():
  import argparse
  from sys import argv
  argv = argv[1:]
  args = argparse.ArgumentParser()
  args.add_argument("--bpath", type=str, dest='hived', default='/home/dev/src/33.develop/hive/build_release_main/programs/hived/hived')
  args.add_argument("--mode", type=str, dest='mode', default='RAND', choices=["RAND","SEQ"])
  args.add_argument("--range-start", type=int, dest='rstart', default=1500, help='in milliseconds')
  args.add_argument("--range-stop", type=int, dest='rstop', default=3000, help='in milliseconds')
  args.add_argument("--tries", type=int, dest='tries', default=100)
  args.add_argument("--halt", type=int, dest='halt', default=0, help='press enter N times at exit before removing datadir')
  args.add_argument("--blocks", type=int, dest='blocks', default=100000)
  args.add_argument("--block-log", type=str, dest='block_log', default='/home/dev/src/hive-data-segfault/blockchain/block_log')
  args.add_argument("--purge", type=bool, dest='purge', default=True, help="if false, does not delete previous datadir")
  return args.parse_args(argv)

def rand(start : int, stop : int):
  from random import randint
  while True:
    yield randint(start, stop)

def seq(start : int, stop : int):
  for i in range(start, stop, 100):
    yield i

def prepare_env( args, datadir ):
  from os import mkdir, symlink
  from os.path import exists, join
  from shutil import rmtree

  print("***prepare_env-start***")
  config_dest = join(datadir, "config.ini")
  print("***{}***".format(config_dest))

  from configini import config
  cfg = config( False,
    witness = None,
    private_key = None,
    plugin = [ 
      'webserver', 'p2p', 
      'json_rpc', 'database_api', 
      'condenser_api', 'block_api', 
      'market_history', 'market_history_api', 
      'account_history_rocksdb', 'account_history_api', 
      'transaction_status', 'transaction_status_api' 
    ],
    stop_replay_at_block=str(args.blocks),
    webserver_http_endpoint='127.0.0.1:20000',
    webserver_ws_endpoint='127.0.0.1:20001'
  )
  return [ args.hived, "--replay-blockchain", "-d", datadir, "--force-open"]

def check( args : str, sleep_time, cnt : int ):
  from subprocess import signal, Popen
  from time import sleep

  print(f"runnig hived:\t{' '.join(args)}")

  with open(f"log_dump/{cnt}.{sleep_time}.log", 'w') as STDOUT:
    #with open(f"log_dump/stderr_{sleep_time}.log", 'w') as STDERR:
      with Popen(args=args, bufsize=0, stdout=STDOUT, stderr=STDOUT) as process:
        print(f'running for {sleep_time}s ...')
        sleep(sleep_time / 1000.0)
        process.send_signal(signal.SIGTERM)
        print('awaiting kill 5s ...')
        return process.wait(5)


def call( bool_value ):
  from requests import get as rget
  while bool_value[0]:
    try:
      _ = rget( 
        f'http://localhost:20000', 
        data='{"jsonrpc":"2.0", "method":"condenser_api.get_dynamic_global_properties", "params":[], "id":1}', 
        headers={'Content-Type': 'application/json'}
      )
    except Exception as e:
      print(e)
      pass

if __name__ == "__main__":

  RUN = [True]
  cfg = get_config()
  generator = None
  start = cfg.rstart
  stop = cfg.rstop
  count = stop - start
  futures = []

  try:
    if cfg.mode == 'RAND':
      count = cfg.tries
      generator = rand(start, stop)
    elif cfg.mode == 'SEQ':
      generator = seq(start, stop)

    _max_workers = 5
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor( max_workers = _max_workers ) as pool:
      futures = [ pool.submit( call, RUN ) for _ in range( _max_workers ) ]

      try:
        args = prepare_env(cfg, '/home/dev/src/hive-data-segfault')
        for cnt in range(count):
          retcode = check( args, next(generator), cnt )
          if retcode != 0:
            print(f"returned non-0 value: {retcode}")
            break
      except Exception as e:
        print(e)
      finally:
        RUN[0] = False

      if cfg.halt > 0:
        for __ in range(cfg.halt):
          input(f"HALT {__ + 1}/{cfg.halt}")
  except:
    print("exception")