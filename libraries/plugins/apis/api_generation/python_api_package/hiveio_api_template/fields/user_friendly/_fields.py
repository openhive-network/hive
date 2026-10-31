from __future__ import annotations

from hiveio_api.fields import validation

from ._simple_init_validator import SimpleInitValidator

__all__ = [
    "AccountName",
    "BlockId",
    "CustomIdType",
    "HardforkVersion",
    "Hex",
    "HiveInt",
    "Identifier",
    "Int16t",
    "Int64t",
    "NodeType",
    "OptionallyEmptyAccountName",
    "Permlink",
    "PrivateKey",
    "PublicKey",
    "Revision",
    "Sha256",
    "Signature",
    "TransactionId",
    "Uint8t",
    "Uint16t",
    "Uint32t",
    "Uint64t",
    "Version",
    "WitnessUrl",
]


class AccountName(SimpleInitValidator, str, form=validation.AccountName):
    """Name of an account."""


class OptionallyEmptyAccountName(SimpleInitValidator, str, form=validation.OptionallyEmptyAccountName):
    """Name of an account or an empty string."""


class CustomIdType(SimpleInitValidator, str, form=validation.CustomIdType):
    """Identifier of a `custom_json`."""


class Permlink(SimpleInitValidator, str, form=validation.Permlink):
    """Permanent link of a post or a comment."""


class PublicKey(SimpleInitValidator, str, form=validation.PublicKey):
    """Public key in the base58 form with the prefix (`STM...`)."""


class PrivateKey(SimpleInitValidator, str, form=validation.PrivateKey):
    """Private key in the WIF form."""


class WitnessUrl(SimpleInitValidator, str, form=validation.WitnessUrl):
    """URL of a witness."""


class NodeType(SimpleInitValidator, str, form=validation.NodeType):
    """Type of a network of hived - `mainnet`, `testnet` or `mirrornet`."""


class Hex(SimpleInitValidator, str, form=validation.Hex):
    """Hexadecimal string."""


class Sha256(SimpleInitValidator, str, form=validation.Sha256):
    """SHA-256 hash as a hexadecimal string."""


class Signature(SimpleInitValidator, str, form=validation.Signature):
    """Signature as a hexadecimal string."""


class Identifier(SimpleInitValidator, str, form=validation.Identifier):
    """RIPEMD-160 identifier (of a transaction, a block, a revision) as a hexadecimal string."""


class Version(SimpleInitValidator, str, form=validation.Version):
    """Version - `<major>.<minor>.<patch>`."""


class Uint8t(SimpleInitValidator, int, form=validation.Uint8t):
    """Unsigned integer of 8 bits."""


class Int16t(SimpleInitValidator, int, form=validation.Int16t):
    """Signed integer of 16 bits."""


class Uint16t(SimpleInitValidator, int, form=validation.Uint16t):
    """Unsigned integer of 16 bits."""


class Uint32t(SimpleInitValidator, int, form=validation.Uint32t):
    """Unsigned integer of 32 bits."""


TransactionId = Identifier
BlockId = Identifier
Revision = Identifier
HardforkVersion = Version

# ranges of integers of 64 bits are checked only by validation models
HiveInt = int
Int64t = int
Uint64t = int
