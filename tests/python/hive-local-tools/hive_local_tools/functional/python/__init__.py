from __future__ import annotations

from typing import TYPE_CHECKING, Literal

from schemas.fields.compound import Authority

if TYPE_CHECKING:
    import test_tools as tt
    from schemas.fields.basic import AccountName, PublicKey


def get_authority(
    node: tt.InitNode, account_name: AccountName, authority_level: Literal["posting", "active", "owner"]
) -> Authority:
    return node.api.database.find_accounts(accounts=[account_name]).accounts[0][authority_level]


def basic_authority(key: PublicKey) -> Authority:
    return Authority(weight_threshold=1, account_auths=[], key_auths=[[key, 1]])


def generate_block(node: tt.InitNode, number: int, miss_blocks: int = 0) -> None:
    node.api.debug_node.debug_generate_blocks(
        debug_key="",
        count=number,
        skip=0,
        miss_blocks=miss_blocks,
        edit_if_needed=False,
    )
