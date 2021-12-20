import threading
import time


def doit(arg):
    t = threading.currentThread()
    while getattr(t, "do_run", True):
        print("working on %s" % arg)
        time.sleep(1)
    print("Stopping as you wish.")


def main():
    t = threading.Thread(target=doit, args=("task",))
    t.start()
    time.sleep(5)
    t.do_run = False


if __name__ == "__main__":
    main()