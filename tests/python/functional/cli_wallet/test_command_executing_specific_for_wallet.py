from __future__ import annotations

from datetime import timedelta

import pytest

import test_tools as tt


@pytest.fixture()
def unconfigured_offline_wallet() -> tt.OldWallet:
    return tt.OldWallet(preconfigure=False)


@pytest.fixture()
def unconfigured_online_wallet(node) -> tt.OldWallet:
    return tt.OldWallet(attach_to=node, preconfigure=False)


@pytest.fixture()
def configured_offline_wallet() -> tt.OldWallet:
    return tt.OldWallet()


@pytest.fixture(params=["unconfigured_online_wallet", "unconfigured_offline_wallet"])
def unconfigured_wallet(request) -> tt.OldWallet:
    return request.getfixturevalue(request.param)


@pytest.fixture(params=["wallet", "configured_offline_wallet"])
def configured_wallet(request) -> tt.OldWallet:
    return request.getfixturevalue(request.param)


def test_if_state_is_new_after_first_start(unconfigured_wallet: tt.OldWallet) -> None:
    assert unconfigured_wallet.api.is_new() is True
    assert unconfigured_wallet.api.is_locked() is True


def test_if_state_is_locked_after_first_password_set(unconfigured_wallet: tt.OldWallet) -> None:
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    assert unconfigured_wallet.api.is_new() is False
    assert unconfigured_wallet.api.is_locked() is True


def test_if_state_is_unlocked_after_entering_password(unconfigured_wallet: tt.OldWallet) -> None:
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.unlock(unconfigured_wallet.DEFAULT_PASSWORD)
    assert unconfigured_wallet.api.is_new() is False
    assert unconfigured_wallet.api.is_locked() is False


def test_if_state_is_locked_after_entering_password(unconfigured_wallet: tt.OldWallet) -> None:
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.unlock(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.lock()
    assert unconfigured_wallet.api.is_new() is False
    assert unconfigured_wallet.api.is_locked() is True


def test_if_state_is_locked_after_close_and_reopen(unconfigured_wallet: tt.OldWallet) -> None:
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.unlock(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.restart(preconfigure=False)
    assert unconfigured_wallet.api.is_new() is False
    assert unconfigured_wallet.api.is_locked() is True


def test_save_wallet_to_file(configured_wallet: tt.OldWallet) -> None:
    wallet_file_path = configured_wallet.directory / "test_file.json"
    configured_wallet.api.save_wallet_file(str(wallet_file_path))
    assert wallet_file_path.exists()


def test_load_wallet_from_file(configured_wallet: tt.OldWallet) -> None:
    wallet_file_path = configured_wallet.directory / "test_file.json"
    configured_wallet.api.save_wallet_file(str(wallet_file_path))
    assert configured_wallet.api.load_wallet_file(str(wallet_file_path)) is True


def test_get_prototype_operation(configured_wallet: tt.OldWallet) -> None:
    assert "comment" in configured_wallet.api.get_prototype_operation("comment_operation")


def test_about(configured_wallet: tt.OldWallet) -> None:
    assert "blockchain_version" in configured_wallet.api.about()
    assert "client_version" in configured_wallet.api.about()


def test_normalize_brain_key(configured_wallet: tt.OldWallet) -> None:
    assert configured_wallet.api.normalize_brain_key("     mango Apple CHERRY ") == "MANGO APPLE CHERRY"


def test_list_keys_and_import_key(unconfigured_wallet: tt.OldWallet) -> None:
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.unlock(unconfigured_wallet.DEFAULT_PASSWORD)
    keys = unconfigured_wallet.api.list_keys()
    assert len(keys) == 0

    unconfigured_wallet.api.import_key(tt.Account("initminer").private_key)
    unconfigured_wallet.api.import_key(tt.Account("alice").private_key)

    keys = unconfigured_wallet.api.list_keys()
    assert len(keys) == 2
    assert keys[0][1] == tt.Account("alice").private_key
    assert keys[1][1] == tt.Account("initminer").private_key


def test_import_keys(unconfigured_wallet: tt.OldWallet) -> None:
    unconfigured_wallet.api.set_password(unconfigured_wallet.DEFAULT_PASSWORD)
    unconfigured_wallet.api.unlock(unconfigured_wallet.DEFAULT_PASSWORD)
    keys = unconfigured_wallet.api.list_keys()
    assert len(keys) == 0

    unconfigured_wallet.api.import_keys([tt.Account("initminer").private_key, tt.Account("alice").private_key])
    keys = unconfigured_wallet.api.list_keys()
    assert len(keys) == 2
    assert keys[0][1] == tt.Account("alice").private_key
    assert keys[1][1] == tt.Account("initminer").private_key


def test_generate_keys(configured_wallet: tt.OldWallet) -> None:
    result = configured_wallet.api.get_private_key_from_password("hulabula", "owner", "apricot")
    assert len(result) == 2

    assert result[0] == "STM5Fuu7PnmJh5dxguaxMZU1KLGcmAh8xgg3uGMUmV9m62BDQb3kB"
    assert result[1] == "5HwfhtUXPdxgwukwfjBbwogWfaxrUcrJk6u6oCfv4Uw6DZwqC1H"


def test_get_private_key_related_to_public_key(configured_wallet: tt.OldWallet) -> None:
    public_key = tt.Account("initminer").public_key
    private_key = tt.Account("initminer").private_key
    assert configured_wallet.api.get_private_key(public_key) == private_key


def test_suggest_brain_key(configured_wallet: tt.OldWallet) -> None:
    result = configured_wallet.api.suggest_brain_key()
    brain_priv_key = result["brain_priv_key"].split(" ")

    assert len(brain_priv_key) == 16
    assert len(result["wif_priv_key"]) == 51
    assert result["pub_key"].startswith("STM")


def test_set_transaction_expiration() -> None:
    # The new "node" with the used methods "wait_for_block_with_number" and "ebug_generate_blocks" was created
    # in order to ensure the repeatability of the generated block sequence. This makes it possible to synchronize
    # the time used in the test and its efficient execution.

    node = tt.InitNode()
    node.run()
    node.wait_for_block_with_number(3)
    node.api.debug_node.debug_generate_blocks(
        debug_key=tt.Account("initminer").private_key, count=20, skip=0, miss_blocks=0
    )

    response = node.api.block.get_block(block_num=23)["block"]
    last_block_time_point = tt.Time.parse(response["timestamp"])

    wallet = tt.OldWallet(attach_to=node)

    set_expiration_time = 1000
    wallet.api.set_transaction_expiration(set_expiration_time)
    transaction = wallet.api.create_account("initminer", "alice", "{}", False)
    expiration_time_point = tt.Time.parse(transaction["expiration"])
    expiration_time = expiration_time_point - last_block_time_point
    assert expiration_time == timedelta(seconds=set_expiration_time)


def test_serialize_transaction(configured_wallet: tt.OldWallet, node: tt.InitNode) -> None:
    wallet_temp = tt.OldWallet(attach_to=node)
    transaction = wallet_temp.api.create_account("initminer", "alice", "{}", False)
    serialized_transaction = configured_wallet.api.serialize_transaction(transaction)
    assert serialized_transaction != "00000000000000000000000000"


def test_get_encrypted_memo_and_decrypt_memo(configured_wallet: tt.OldWallet, node: tt.InitNode) -> None:
    wallet_temp = tt.OldWallet(attach_to=node)
    wallet_temp.api.create_account("initminer", "alice", "{}")
    encrypted = wallet_temp.api.get_encrypted_memo("alice", "initminer", "#this is memo")
    assert configured_wallet.api.decrypt_memo(encrypted) == "this is memo"


def test_exit_from_wallet(configured_wallet: tt.OldWallet) -> None:
    configured_wallet.api.exit()
