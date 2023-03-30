from datetime import datetime
import re

from pydantic import BaseModel, PositiveInt, validator, ConstrainedStr, ConstrainedInt


class HiveDateTime(datetime):
    @classmethod
    @validator('isoformat')
    def check_custom_format(cls, v):
        try:
            datetime.strptime(v, '%Y-%m-%dT%H:%M:%S')
        except ValueError:
            raise ValueError('date must be in format %Y-%m-%dT%H:%M:%S')
        return v


class HiveInt(ConstrainedInt):
    ge = 0

    @classmethod
    @validator('hiveformat')
    def check_int_hive_format(cls, v):
        try:
            hive_int = int(v)
        except ValueError:
            raise ValueError('That is not int, and cant convert it to int')
        return hive_int


class RegexName(ConstrainedStr):
    regex = re.compile(r'[a-z][a-z0-9\-]+[a-z0-9]')
    min_length = 3
    max_length = 16


class RegexKey(ConstrainedStr):
    regex = re.compile(r'^(?:STM|TST)[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]{7,51}$')


class Authority(BaseModel):
    weight_threshold: PositiveInt
    account_auths: list[tuple[RegexName, PositiveInt]]
    key_auths: list[tuple[RegexKey, PositiveInt]]


class Manabar(BaseModel):
    current_mana: int
    last_update_time: int


class AssetHive(BaseModel):
    amount: int
    precision: int
    nai: str

    @classmethod
    @validator('nai')
    def check_nai(cls, nai):
        if nai != "@@000000021":
            raise ValueError("Invalid nai!")
        return nai


class AssetHbd(BaseModel):
    amount: int
    precision: int
    nai: str

    @classmethod
    @validator('nai')
    def check_nai(cls, nai):
        if nai != "@@000000013":
            raise ValueError("Invalid nai!")
        return nai


class AssetVests(BaseModel):
    amount: int
    precision: int
    nai: str

    @classmethod
    @validator('nai')
    def check_nai(cls, nai):
        if nai != "@@000000037":
            raise ValueError("Invalid nai!")
        return nai


class DelayedVotes(BaseModel):
    time: HiveDateTime
    val: int


class HardForkVersion(ConstrainedStr):
    regex = re.compile(r'^(?:(?:[1-9][0-9]*)|0)\.[0-9]+$')


class Hex(ConstrainedStr):
    pass


class Sha256(ConstrainedStr):
    regex = re.compile(r'^[0-9a-fA-F]*$')
    min_length = 64
    max_length = 64


class HiveVersion(BaseModel):
    blockchain_version: HardForkVersion
    hive_revision: Hex
    fc_revision: Hex
    chain_id: Sha256
    node_type: str  # constr(reqex=r'^(mainnet|testnet|mirrornet)$')


class HbdExchangeRate(BaseModel):
    """
    Need to get more information
    """
    pass


class LegacyAssetHbd(ConstrainedStr):
    regex = re.compile(r'^[0-9]+\.[0-9]{3} (?:HBD|TBD)$')

    @staticmethod
    def symbol():
        pass


