from pydantic import BaseModel, PositiveInt, validator, ConstrainedStr
from datetime import datetime

import re


class HiveDateTime(datetime):
    @classmethod
    @validator('isoformat')
    def check_custom_format(cls, v):
        try:
            datetime.strptime(v, '%Y-%m-%dT%H:%M:%S')
        except ValueError:
            raise ValueError('date must be in format %Y-%m-%dT%H:%M:%S')
        return v


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






