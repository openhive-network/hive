from __future__ import annotations

from hive_local_tools.functional.python.operation import Operation, create_transaction_with_any_operation
from schemas.operations.custom_json_operation import CustomJsonOperation
from schemas.operations.custom_operation import CustomOperation


class Custom(Operation):
    def generate_transaction(self, required_auths: list[str], id_: int, data: str) -> dict:
        return create_transaction_with_any_operation(
            self._wallet, [CustomOperation(required_auths=required_auths, id_=id_, data=data)], broadcast=False
        )

    def sign_transaction(self, transaction: dict, broadcast: bool = True) -> dict:
        return self._wallet.api.sign_transaction(transaction, update_tapos=False, broadcast=broadcast)


class CustomJson(Operation):
    def generate_transaction(
        self, required_auths: list[str], required_posting_auths: list[str], id_: int, json_: str
    ) -> dict:
        return create_transaction_with_any_operation(
            self._wallet,
            [
                CustomJsonOperation(
                    required_auths=required_auths,
                    required_posting_auths=required_posting_auths,
                    id_=str(id_),
                    json_=json_,
                )
            ],
            broadcast=False,
        )

    def sign_transaction(self, transaction: dict, broadcast: bool = True) -> dict:
        return self._wallet.api.sign_transaction(transaction, update_tapos=False, broadcast=broadcast)
