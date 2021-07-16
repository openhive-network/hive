from test_tools import *

def test_exit_before_sync(world : World):
  '''
  with specified '--exit-before-sync' flag:
  1. check exiting after dumping snapshot
  2. check exiting after loading snapshot
  3. check after replaying
  4. check after load+replay combo (it should exit after replay)
  '''