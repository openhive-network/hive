import datetime


def check_hive_datetime(_checker, instance):
    try:
        datetime.datetime.strptime(instance, '%Y-%m-%dT%H:%M:%S')
        return True
    except ValueError:
        return False
    except TypeError:
        return False
