def check_sell_price(node, base, quote):
    assert "sell_price" in node
    _sell_price = node["sell_price"]

    assert "base" in _sell_price
    _sell_price["base"] == base

    assert "quote" in _sell_price
    _sell_price["quote"] == quote


def check_recurrence_transfer_data(node):
    _op = node["operations"][0]
    assert _op[0] == "recurrent_transfer"
    return _op[1]


def check_recurrence_transfer(node, _from, to, amount, memo, recurrence, executions_key, executions_number):
    assert node["from"] == _from
    assert node["to"] == to
    assert node["amount"] == amount
    assert node["memo"] == memo
    assert node["recurrence"] == recurrence
    assert node[executions_key] == executions_number


def check_ask(node, base, quote):
    assert "order_price" in node
    _order_price = node["order_price"]

    assert "base" in _order_price
    _order_price["base"] == base

    assert "quote" in _order_price
    _order_price["quote"] == quote


def get_key(node_name, result):
    _key_auths = result[node_name]["key_auths"]
    assert len(_key_auths) == 1
    __key_auths = _key_auths[0]
    assert len(__key_auths) == 2
    return __key_auths[0]


def check_key(node_name, result, key=None):
    _key = get_key(node_name, result)
    assert _key == key


def check_keys(result, key_owner, key_active, key_posting, key_memo):
    check_key("owner", result, key_owner)
    check_key("active", result, key_active)
    check_key("posting", result, key_posting)
    assert result["memo_key"] == key_memo


def create_accounts(wallet, creator, accounts):
    for account in accounts:
        wallet.api.create_account(creator, account, "{}")
