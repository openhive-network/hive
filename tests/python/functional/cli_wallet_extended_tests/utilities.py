from __future__ import annotations

import test_tools as tt


def check_sell_price(
    node: tt.InitNode, base: tt.Asset.HbdT | tt.Asset.HiveT, quote: tt.Asset.HbdT | tt.Asset.HiveT
) -> None:
    assert "sell_price" in node
    _sell_price = node["sell_price"]

    assert "base" in _sell_price
    assert tt.Asset.from_legacy(_sell_price["base"]) == base

    assert "quote" in _sell_price
    assert tt.Asset.from_legacy(_sell_price["quote"]) == quote


def check_recurrent_transfer_data(node: tt.InitNode) -> None:
    _op = node["operations"][0]
    assert _op[0] == "recurrent_transfer"
    return _op[1]


def check_recurrent_transfer(
    node,
    _from: str,
    to: str,
    amount: tt.Asset.HiveT | tt.Asset.HbdT,
    memo: str,
    recurrence: int,
    executions_key: str,
    executions_number: int,
    pair_id: int,
) -> None:
    assert node["from"] == _from
    assert node["to"] == to
    assert tt.Asset.from_legacy(node["amount"]) == amount
    assert node["memo"] == memo
    assert node["recurrence"] == recurrence
    assert node[executions_key] == executions_number

    # pair_id check
    extension_pair_id = 0
    if "extensions" in node:
        for ext in node["extensions"]:
            if ext["type"] == "recurrent_transfer_pair_id":
                extension_pair_id = ext["value"]["pair_id"]
    assert extension_pair_id == pair_id, f"Expected pair_id={pair_id}"


def check_ask(node: tt.InitNode, base: tt.Asset.HbdT | tt.Asset.HiveT, quote: tt.Asset.HbdT | tt.Asset.HiveT) -> None:
    assert "order_price" in node
    _order_price = node["order_price"]

    assert "base" in _order_price
    assert tt.Asset.from_legacy(_order_price["base"]) == base

    assert "quote" in _order_price
    assert tt.Asset.from_legacy(_order_price["quote"]) == quote


def get_key(node_name: str, result: dict) -> list:
    _key_auths = result[node_name]["key_auths"]
    assert len(_key_auths) == 1
    __key_auths = _key_auths[0]
    assert len(__key_auths) == 2
    return __key_auths[0]


def check_key(node_name: str, result: dict, key: str | None = None) -> None:
    _key = get_key(node_name, result)
    assert _key == key


def check_keys(result: dict, key_owner: str, key_active: str, key_posting: str, key_memo: str) -> None:
    check_key("owner", result, key_owner)
    check_key("active", result, key_active)
    check_key("posting", result, key_posting)
    assert result["memo_key"] == key_memo


def create_accounts(wallet: tt.OldWallet, creator: str, accounts: list) -> None:
    for account in accounts:
        wallet.api.create_account(creator, account, "{}")
