from __future__ import annotations

from hive_local_tools.functional.python.operation import Operation
from schemas.operations.custom_json_operation import CustomJsonOperation
from schemas.operations.custom_operation import CustomOperation


class Custom(Operation):
    @staticmethod
    def generate_operation(required_auths: list[str], id_: int, data: list[str]) -> CustomOperation:
        return CustomOperation(required_auths=required_auths, id_=id_, data=data)


@staticmethod
class CustomJson(Operation):
    @staticmethod
    def generate_operation(
        required_auths: list[str], required_posting_auths: list[str], id_: str, json_: str
    ) -> CustomJsonOperation:
        return CustomJsonOperation(
            required_auths=required_auths, required_posting_auths=required_posting_auths, id_=id_, json_=json_
        )
