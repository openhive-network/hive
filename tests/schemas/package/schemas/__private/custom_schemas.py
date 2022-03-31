from abc import abstractmethod
import typing
from typing import Dict

from schemas.__private.fundamental_schemas import Array, ArrayStrict, Date, Int, Map, Schema, String


class CustomSchema(Schema):
    @abstractmethod
    def _define_schema(self) -> Schema:
        pass

    def _create_core_of_schema(self) -> Dict[str, typing.Any]:
        return self._define_schema()._create_schema()


class AssetHbd(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            'amount': String(),
            'precision': Int(enum=[3]),
            'nai': String(pattern='@@000000013'),
        })


class AssetHive(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            'amount': String(),
            'precision': Int(enum=[3]),
            'nai': String(pattern='@@000000021'),
        })


class AssetVests(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            'amount': String(),
            'precision': Int(enum=[6]),
            'nai': String(pattern='@@000000037'),
        })


class Authority(CustomSchema):
    def _define_schema(self) -> Schema:
        return Map({
            'weight_threshold': Int(),
            'account_auths': Array(
                ArrayStrict(
                    String(),
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
        return String(pattern=r'^(?:STM|TST)[A-Za-z0-9]{50}$')


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
            'creator': String(),
            'receiver': String(),
            'start_date': Date(),
            'end_date': Date(),
            'daily_pay': AssetHbd(),
            'subject': String(),
            'permlink': String(),
            'total_votes': Int(),
            'status': String(),
        })


class Version(CustomSchema):
    def _define_schema(self) -> Schema:
        return String(pattern=r'^\d+\.\d+\.\d+$')
