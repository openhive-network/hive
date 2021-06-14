import os
import sys
import logging
import datetime

log = logging.getLogger("Cli_wallet")

def init_logger(_file_name):
    global log
    log.handlers = []
    formater = '%(asctime)s [%(levelname)s] %(message)s'
    stdh = logging.StreamHandler(sys.stdout)
    stdh.setFormatter(logging.Formatter(formater))
    log.addHandler(stdh)
    log.setLevel(logging.INFO)

    data = os.path.split(_file_name)
    log_dir = os.environ.get("TEST_LOG_DIR", None)
    if log_dir:
        path = log_dir 
    else:
        path = data[0] + "/logs/"
    file_name = "cli_wallet.log"
    if path and not os.path.exists(path):
        os.makedirs(path)
    full_path = path+"/"+file_name
    fileh = logging.FileHandler(full_path)
    fileh.setFormatter(logging.Formatter(formater))
    log.addHandler(fileh)

