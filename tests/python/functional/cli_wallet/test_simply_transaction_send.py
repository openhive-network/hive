from __future__ import annotations

from datetime import timedelta
from typing import TYPE_CHECKING

import test_tools as tt
import wax
from schemas.fields.hive_int import HiveInt
from schemas.operations.delete_comment_operation import DeleteCommentOperation
from schemas.operations.representations import HF26Representation
from schemas.transaction import Transaction

if TYPE_CHECKING:
    from schemas.operations import AnyOperation


def test_first():
    node = tt.InitNode()
    node.run()

    b_wallet = tt.BeekeeperWallet()

    node.api.database.get_dynamic_global_properties()
    node_config = node.api.database.get_config()
    account = tt.Account("initminer")
    operation = DeleteCommentOperation(author=account.name, permlink="permlink")
    transaction = generate_transaction_template(node, 0.0)
    transaction.add_operation(operation)
    wax_result = wax.calculate_sig_digest(
        transaction.json(by_alias=True).encode("ascii"), node_config.HIVE_CHAIN_ID.encode("ascii")
    )
    if (
        wax_result.exception_message != b""
    ):  # to sprawdzenie się przydaje jakby był błąd w schemach albo przy tworzeniu operacji
        raise Exception(wax_result.exception_message)  # noqa
    sig_digest = wax_result.result.decode("ascii")
    signature = b_wallet.beekeeper_wallet.sign_digest(sig_digest=sig_digest, key=__normalize_key(account.public_key))
    signature = transaction.signatures.append(signature)  # dorzucamy sygnaturę
    node.api.wallet_bridge.broadcast_transaction(transaction)
    b_wallet.beekeeper_wallet.generate_key()


class SimpleTransaction(Transaction):
    def add_operation(self, op: AnyOperation) -> None:
        self.operations.append(HF26Representation(type=op.get_name_with_suffix(), value=op))


def get_tapos_data(block_id: str) -> wax.python_ref_block_data:
    return wax.get_tapos_data(block_id.encode())  # type: ignore[no-any-return]


def generate_transaction_template(node: tt.InitNode, time_offset: float) -> SimpleTransaction:
    gdpo = node.api.database.get_dynamic_global_properties()

    # get dynamic global properties
    # Fixme: after add block_log_util.get_block_ids methods to test_tools
    block_id = gdpo.head_block_id
    # block_id: str = "0000018fdd29ffba68dff6ba9619c89618787734"  # single sign
    # block_id: str = "0000018f6c609bda7890f4e3b750cab5d88a9e8c"  # open sign
    # block_id: str = "0000018f75b4c21ecb7b00e001924ea9be0d7790"  # multi sign

    # set header
    tapos_data = get_tapos_data(block_id)
    ref_block_num = tapos_data.ref_block_num
    ref_block_prefix = tapos_data.ref_block_prefix

    assert ref_block_num >= 0, f"ref_block_num value `{ref_block_num}` is invalid`"
    assert ref_block_prefix > 0, f"ref_block_prefix value `{ref_block_prefix}` is invalid`"

    return SimpleTransaction(
        ref_block_num=HiveInt(ref_block_num),
        ref_block_prefix=HiveInt(ref_block_prefix),
        expiration=gdpo.time + timedelta(seconds=time_offset) + timedelta(minutes=59),
        extensions=[],
        signatures=[],
        operations=[],
    )


def __normalize_key(pubkey):
    return pubkey[3:]

    """
    gdpo = remote_node.api.database.get_dynamic_global_properties()
    node_config = remote_node.api.database.get_config()


    trx: SimpleTransaction = generate_transaction_template(gdpo)
    trx.add_operation(some_operation)
    wax_result = wax.calculate_sig_digest(trx.json(by_alias=True).encode("ascii"), node_config.HIVE_CHAIN_ID.encode("ascii"))
    if wax_result.error is not None:  # to sprawdzenie się przydaje jakby był błąd w schemach albo przy tworzeniu operacji
         raise Exception(wax_result.error)
    sig_digest = wax_result.result.decode("ascii")
    signature = (await beekeeper.api.sign_digest(sig_digest=sig_digest, public_key=account.public_key)).signature
    trx.signatures.append(signature)  # dorzucamy sygnaturę
    node.api.broadcast.broadcast_transaction(trx=trx)  # broadcastujemy
    """
    return None
