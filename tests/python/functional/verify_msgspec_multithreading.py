from __future__ import annotations

import json
import threading
from datetime import datetime
from typing import Annotated, Any, Final, TypeVar

import msgspec

ACCOUNT_NAME_SEGMENT_REGEX: Final[str] = r"[a-z][a-z0-9\-]+[a-z0-9]"
BASE_58_REGEX: Final[str] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

Int16t = Annotated[int, msgspec.Meta(ge=-32768, le=32767)]
DEFAULT_WEIGHT: Final[Int16t] = Int16t(0)

CustomIdType = Annotated[str, msgspec.Meta(max_length=32)]


AccountNameMsgspec = Annotated[
    str,
    msgspec.Meta(
        max_length=16, min_length=3, pattern=rf"^{ACCOUNT_NAME_SEGMENT_REGEX}(?:\.{ACCOUNT_NAME_SEGMENT_REGEX})*$"
    ),
]


class OperationMsgspec(msgspec.Struct):
    """Base class for all operations to provide valid json serialization"""

    @classmethod
    def get_name(cls) -> str:
        """
        Get the name of the operation.

        e.g. `transfer` for `TransferOperation`
        """
        return cls.__operation_name__

    @classmethod
    def get_name_with_suffix(cls) -> str:
        """
        Get the name of the operation with the `_operation` suffix.

        e.g. `transfer_operation` for `TransferOperation`
        """
        return f"{cls.get_name()}_operation"

    @classmethod
    def offset(cls) -> int:
        return cls.__offset__

    def get_hf26_rep(self):
        class HF26Representation(msgspec.Struct, tag=self.get_name_with_suffix()):
            value: self.__class__

        return HF26Representation(value=self)


class VoteOperation(OperationMsgspec):
    __operation_name__ = "vote"
    __offset__ = 0

    voter: AccountNameMsgspec
    author: AccountNameMsgspec
    permlink: str
    weight: Int16t = DEFAULT_WEIGHT


class CommentOperation(OperationMsgspec):
    __operation_name__ = "comment"
    __offset__ = 1

    parent_author: AccountNameMsgspec
    parent_permlink: str
    author: AccountNameMsgspec
    permlink: str
    title: str
    body: str
    json_metadata: str


OperationTypes = TypeVar("OperationTypes", bound=CommentOperation | VoteOperation)


class HF26RepresentationVoteOperation(msgspec.Struct, tag="vote_operation"):
    value: VoteOperation


class HF26RepresentationCommentOperation(msgspec.Struct, tag="comment_operation"):
    value: CommentOperation


def encode(obj):
    return json.dumps(msgspec.to_builtins(obj))


def decode(msg, type=Any):
    return msgspec.convert(json.loads(msg), type=type)


class SimplyTransactionMsgspec(msgspec.Struct):
    ref_block_num: int
    ref_block_prefix: int
    expiration: datetime
    extensions: list[Any]
    signatures: list[str]
    operations: list[HF26RepresentationCommentOperation | HF26RepresentationVoteOperation] = []

    def add_operation(self, operation: OperationMsgspec) -> None:
        self.operations.append(operation.get_hf26_rep())

    def json(self):
        return encode(self)


def make_trx(ref_block_num):
    for weignt in range(100):
        trx = SimplyTransactionMsgspec(
            ref_block_num=ref_block_num,
            ref_block_prefix=2605225370,
            expiration=datetime.now(),
            signatures=[
                "1f7f017a2a69b64c968536b64495c9e0a6d0ff29c543131b7d9fc4ecb67e51e028258ce7aedd202ea709f9a1fc1dfe143300b040e3ccef0a66f8b367cdc50f6be5"
            ],
            extensions=[],
        )
        vote_operation = VoteOperation(voter="alice", author="bob", permlink="permlink", weight=weignt)
        trx.add_operation(vote_operation)
        # print(trx)


if __name__ == "__main__":
    # Lista do przechowywania wątków
    threads = []

    # Tworzenie i uruchamianie 20 wątków w pętli
    for i in range(20):
        thread = threading.Thread(target=make_trx, args=(i,))
        threads.append(thread)
        thread.start()

    # Oczekiwanie na zakończenie wszystkich wątków
    for thread in threads:
        thread.join()

    print("Wszystkie wątki zakończyły działanie!")
