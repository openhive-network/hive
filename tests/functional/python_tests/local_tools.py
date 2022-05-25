from datetime import datetime


def parse_datetime(datetime_: str) -> datetime:
    return datetime.strptime(datetime_, '%Y-%m-%dT%H:%M:%S')
