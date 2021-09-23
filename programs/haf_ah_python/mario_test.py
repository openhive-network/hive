#!/usr/bin/python3

import multiprocessing
import queue
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from collections import deque

def worker(name, que, sleep):
  time.sleep(sleep)
  print("worker: {}".format(name))
  que.put("%d is done" % name)
  return 1

def recevier(q):
  try:
    time.sleep(2)
    _result = q.get(True,1)
    print("recevier: {}".format(_result))
  except queue.Empty:
    print("empty!")
  return 2

def a(result_objs, pool):
  result_objs.append( pool.apply_async(worker, (33, q,3)) )
  result_objs.append( pool.apply_async(worker, (34, q,2)) )
  result_objs.append( pool.apply_async(worker, (35, q,1)) )
  result_objs.append( pool.apply_async(worker, (36, q,3)) )

def b(result_objs, pool):
  result_objs.append( pool.apply_async(recevier, [q]) )
  result_objs.append( pool.apply_async(recevier, [q]) )
  result_objs.append( pool.apply_async(recevier, [q]) )

if __name__ == '__main__':
    print("start")
    pool = multiprocessing.Pool(processes=10)
    m = multiprocessing.Manager()
    q = m.Queue()

    result_objs = []

    result_objs.append( pool.apply_async(worker, (33, q,3)) )
    result_objs.append( pool.apply_async(worker, (34, q,2)) )
    result_objs.append( pool.apply_async(worker, (35, q,1)) )
    result_objs.append( pool.apply_async(worker, (36, q,3)) )

    result_objs.append( pool.apply_async(recevier, [q]) )
    result_objs.append( pool.apply_async(recevier, [q]) )
    result_objs.append( pool.apply_async(recevier, [q]) )
    # _futures = []
    # with ThreadPoolExecutor(max_workers = 2) as executor:
    #   _futures.append(executor.submit(a, result_objs, pool))
    #   _futures.append(executor.submit(b, result_objs, pool))

    # for future in as_completed(_futures):
    #   future.result()

    for result in result_objs:
      print(result.get())

    while not q.empty():
      print(q.get())

    print("end")


