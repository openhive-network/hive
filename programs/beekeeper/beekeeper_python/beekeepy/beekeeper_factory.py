from __future__ import annotations


def beekeeper_factory() -> Beekeeper:
    """Contains logic to setup beekeeper as full service or switches to remote"""
    return ServiceBeekeeper()


def beekeeper_remote_factory(*, url: HttpUrl) -> Beekeeper:
    return RemoteBeekeeper(url=url)
