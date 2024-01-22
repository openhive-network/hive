from __future__ import annotations

from datetime import datetime

from pydantic import validator

from .clive_base_model import CliveBaseModel


class BeekeeperResponse(CliveBaseModel):
    pass


class EmptyResponse(BeekeeperResponse):
    pass


class BeekeeperSession(BeekeeperResponse):
    token: str


class CreateSession(BeekeeperSession):
    pass


class Create(BeekeeperResponse):
    password: str


class ImportKey(BeekeeperResponse):
    public_key: str


class ListKeys(BeekeeperResponse):
    keys: dict[str, str]


class ListWallets(BeekeeperResponse):
    class WalletDetails(CliveBaseModel):
        name: str
        unlocked: bool

    wallets: list[ListWallets.WalletDetails]


class BeekeeperPublicKeyType(CliveBaseModel):
    public_key: str


class GetPublicKeys(BeekeeperResponse):
    keys: list[BeekeeperPublicKeyType]


class SignDigest(BeekeeperResponse):
    signature: str


class SignTransaction(SignDigest):
    pass


class GetInfo(BeekeeperResponse):
    now: datetime
    timeout_time: datetime

    @validator("now", "timeout_time", pre=True)
    @classmethod
    def time_validate(cls, v: str) -> datetime:
        return datetime.fromisoformat(v)
