from __future__ import annotations

import json
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


class PreconfiguredBaseModel(msgspec.Struct):
    class Config:
        # extra = Extra.forbid Chyba nie dotyczy
        allow_population_by_field_name = True
        smart_union = True
        json_encoders = {  # noqa: RUF012
            datetime: lambda x: x.strftime(HIVE_TIME_FORMAT),
            Serializable: lambda x: x.serialize(),
        }

    @classmethod
    def __is_aliased_field_name(cls, field_name: str) -> bool:
        return field_name in {
            "id",
            "from",
            "json",
            "schema",
            "open",
            "field",
            "input",
            "hex",
        }

    def __getitem__(self, key: str) -> Any:
        """
        This allows using any schema from this repo as dictionary
        """
        key = self.__get_field_name(key)
        return getattr(self, key)

    def __setitem__(self, key: str, value: Any) -> None:
        """
        This allows using any schema from this repo as dictionary
        """
        key = self.__get_field_name(key)
        setattr(self, key, value)

    def json(
        self,
        *,
        include: AbstractSetIntStr | MappingIntStrAny | None = None,
        exclude: AbstractSetIntStr | MappingIntStrAny | None = None,
        by_alias: bool = True,  # modified, most of the time we want to dump by alias
        skip_defaults: bool | None = None,
        exclude_unset: bool = False,
        exclude_defaults: bool = False,
        exclude_none: bool = False,
        encoder: Callable[[Any], Any] | None = None,
        models_as_dict: bool = True,
        ensure_ascii: bool = False,  # modified, so unicode characters are not escaped, will properly dump e.g. polish characters
        **dumps_kwargs: Any,
    ) -> str:
        return super().json(
            include=include,
            exclude=exclude,
            by_alias=by_alias,
            skip_defaults=skip_defaults,
            exclude_unset=exclude_unset,
            exclude_defaults=exclude_defaults,
            exclude_none=exclude_none,
            encoder=encoder,
            models_as_dict=models_as_dict,
            ensure_ascii=ensure_ascii,
            **dumps_kwargs,
        )

    def shallow_dict(self) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in self.__dict__.items():
            if value is not None:
                result[key.strip("_")] = value
        return result

    def dict(
        self,
        *,
        include: AbstractSetIntStr | MappingIntStrAny | None = None,
        exclude: AbstractSetIntStr | MappingIntStrAny | None = None,
        by_alias: bool = True,  # modified, most of the time we want to dump by alias
        skip_defaults: bool | None = None,
        exclude_unset: bool = False,
        exclude_defaults: bool = False,
        exclude_none: bool = False,
    ) -> DictStrAny:
        return super().dict(
            include=include,
            exclude=exclude,
            by_alias=by_alias,
            skip_defaults=skip_defaults,
            exclude_unset=exclude_unset,
            exclude_defaults=exclude_defaults,
            exclude_none=exclude_none,
        )

    @classmethod
    def as_strict_model(cls, recursively: bool = True) -> type[Self]:  # noqa: C901
        """
        Generate a BaseModel class with all the same fields like the class on which the method was called but with
        required fields only (no defaults allowed).

        Returns:
            Strict version of the class
        """

        # ellipsis is used to indicate that the field is required
        field_definitions = {field.name: (field.type_, Field(alias=field.alias)) for field in cls.__fields__.values()}

        def process_type(type_: Any) -> Any:
            def resolve_for_all_args(outer_type: Any) -> Any:
                return outer_type[tuple(process_type(arg) for arg in get_args(type_))]

            type_origin = get_origin(type_)
            if (
                type_ in {type(None), typing.Any}
                or type(type_) in {typing.TypeVar, pydantic.fields.FieldInfo}
                or type_origin in {typing.Literal}
            ):
                return type_

            if type_origin is not None:
                if type_origin in {types.UnionType, typing.Union}:
                    return resolve_for_all_args(typing.Union)

                if type_origin in {tuple, typing.Tuple}:
                    return resolve_for_all_args(typing.Tuple)

                if type_origin in {list, typing.List}:
                    return resolve_for_all_args(typing.List)

                if type_origin in {typing.Annotated}:
                    return resolve_for_all_args(typing.Annotated)

            if issubclass(type_, PreconfiguredBaseModel):
                return type_.as_strict_model()
            return type_

        if recursively:
            for field_name, pack in field_definitions.items():
                field_definitions[field_name] = (process_type(pack[0]), ...)

        return create_model(f"{cls.__name__}Strict", **field_definitions, __base__=PreconfiguredBaseModel)  # type: ignore

    def __get_field_name(self, name: str) -> str:
        if not hasattr(self, name) and self.__is_aliased_field_name(name):
            name = f"{name}_"

        assert hasattr(
            self, name
        ), f"`{name}` does not exists in `{self.__class__.__name__}`, available are: {list(self.dict().keys())}"
        return name


BaseModelT = TypeVar("BaseModelT", bound=PreconfiguredBaseModel)


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


if __name__ == "__main__":
    trx = SimplyTransactionMsgspec(
        ref_block_num=1,
        ref_block_prefix=2605225370,
        expiration=datetime.now(),
        signatures=[
            "1f7f017a2a69b64c968536b64495c9e0a6d0ff29c543131b7d9fc4ecb67e51e028258ce7aedd202ea709f9a1fc1dfe143300b040e3ccef0a66f8b367cdc50f6be5"
        ],
        extensions=[],
    )
    vote_operation = VoteOperation(voter="alice", author="bob", permlink="permlink", weight=100)
    comment_operation = CommentOperation(
        parent_author="alice",
        author="bob",
        body="body",
        title="title",
        permlink="permlink",
        parent_permlink="parent-permlink",
        json_metadata="{}",
    )

    trx.add_operation(vote_operation)
    trx.add_operation(comment_operation)

    # json_trx = '{"ref_block_num": 1, "ref_block_prefix": 2605225370, "expiration": "2024-12-05T14:13:13.010621", "extensions": [], "signatures": ["1f7f017a2a69b64c968536b64495c9e0a6d0ff29c543131b7d9fc4ecb67e51e028258ce7aedd202ea709f9a1fc1dfe143300b040e3ccef0a66f8b367cdc50f6be5"], "operations": [{"type": "vote_operation", "value": {"voter": "alice", "author": "bob", "permlink": "permlink", "weight": 100}}, {"type": "comment_operation", "value": {"parent_author": "alice", "parent_permlink": "parent-permlink", "author": "bob", "permlink": "permlink", "title": "title", "body": "body", "json_metadata": "{}"}}]}'

    trx_encode = msgspec.json.encode(trx)
    trx_decode = msgspec.json.decode(trx_encode, type=SimplyTransactionMsgspec)
