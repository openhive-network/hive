from __future__ import annotations

from typing import TYPE_CHECKING, Literal

if TYPE_CHECKING:
    import test_tools as tt
    from schemas.fields.basic import AccountName
    from schemas.fields.compound import Authority


def get_authority(
    node: tt.InitNode, account_name: AccountName, authority_level: Literal["posting", "active", "owner"]
) -> Authority:
    return node.api.database.find_accounts(accounts=[account_name]).accounts[0][authority_level]
