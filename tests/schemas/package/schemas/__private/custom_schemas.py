from abc import abstractmethod
import typing
from typing import Dict

from schemas.__private.fundamental_schemas import Array, ArrayStrict, Date, Int, Map, Schema, Str


class CustomSchema(Schema):
    @abstractmethod
    def _define_schema(self) -> Schema:
        pass

    def _create_core_of_schema(self) -> Dict[str, typing.Any]:
        return self._define_schema()._create_schema()


class AssetHbd(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            'amount': Int(),
            'precision': Int(enum=[3]),
            'nai': Str(pattern='@@000000013'),
        })


class AssetHive(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            'amount': Int(),
            'precision': Int(enum=[3]),
            'nai': Str(pattern='@@000000021'),
        })


class AssetVests(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            'amount': Int(),
            'precision': Int(enum=[6]),
            'nai': Str(pattern='@@000000037'),
        })


class Authority(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            'weight_threshold': Int(),
            'account_auths': Array(
                ArrayStrict(
                    Str(),
                    Int(),
                )
            ),
            'key_auths': Array(
                ArrayStrict(
                    Key(),
                    Int(),
                )
            ),
        })


class Key(CustomSchema):
    def _define_schema(self) -> Schema:
        return Str(pattern=r'^(?:STM|TST)[A-Za-z0-9]{50}$')


class Manabar(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            "current_mana": Int(),
            "last_update_time": Int(),
        })


class Proposal(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            'id': Int(),
            'proposal_id': Int(),
            'creator': Str(),
            'receiver': Str(),
            'start_date': Date(),
            'end_date': Date(),
            'daily_pay': AssetHbd(),
            'subject': Str(),
            'permlink': Str(),
            'total_votes': Int(),
            'status': Str(),
        })


class Version(CustomSchema):
    def _define_schema(self) -> Schema:
        return Str(pattern=r'^\d+\.\d+\.\d+$')
