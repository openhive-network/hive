import datetime


def check_date(_checker, inst):
    try:
        datetime.datetime.strptime(inst, '%Y-%m-%dT%H:%M:%S')
        return True
    except ValueError:
        return False
    except TypeError:
        return False
