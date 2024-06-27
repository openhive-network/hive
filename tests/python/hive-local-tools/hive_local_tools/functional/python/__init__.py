from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from schemas.fields.basic import PublicKey


def get_authority(key: PublicKey) -> dict:
    return {"weight_threshold": 1, "account_auths": [], "key_auths": [[key, 1]]}
