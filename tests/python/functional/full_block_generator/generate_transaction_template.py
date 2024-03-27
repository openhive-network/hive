from __future__ import annotations

from datetime import timedelta
from typing import TYPE_CHECKING

import wax
from schemas.fields.hive_int import HiveInt
from schemas.operations.representations import HF26Representation
from schemas.transaction import Transaction

if TYPE_CHECKING:
    from schemas.apis.database_api import GetDynamicGlobalProperties
    from schemas.operations import AnyOperation


class SimpleTransaction(Transaction):
    def add_operation(self, op: AnyOperation) -> None:
        self.operations.append(HF26Representation(type=op.get_name_with_suffix(), value=op))


def get_tapos_data(block_id: str) -> wax.python_ref_block_data:
    return wax.get_tapos_data(block_id.encode())  # type: ignore[no-any-return]


def generate_transaction_template(gdpo: GetDynamicGlobalProperties) -> SimpleTransaction:
    # get dynamic global properties
    block_id = gdpo.head_block_id

    # set header
    tapos_data = get_tapos_data(block_id)
    ref_block_num = tapos_data.ref_block_num
    ref_block_prefix = tapos_data.ref_block_prefix

    assert ref_block_num >= 0, f"ref_block_num value `{ref_block_num}` is invalid`"
    assert ref_block_prefix > 0, f"ref_block_prefix value `{ref_block_prefix}` is invalid`"

    return SimpleTransaction(
        ref_block_num=HiveInt(ref_block_num),
        ref_block_prefix=HiveInt(ref_block_prefix),
        expiration=gdpo.time + timedelta(minutes=59),
        extensions=[],
        signatures=[],
        operations=[],
    )
