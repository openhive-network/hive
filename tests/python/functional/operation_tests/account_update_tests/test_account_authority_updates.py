from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError

import test_tools as tt
from hive_local_tools.functional.python.operation import get_transaction_timestamp

if TYPE_CHECKING:
    from python.functional.operation_tests.conftest import UpdateAccount


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "authority_type",
    ["active", "posting", "owner"],
)
# When parameter use_account_update2 is False test creates account_update operation otherwise - account_update2
@pytest.mark.parametrize("use_account_update2", [(True, False)])
def test_update_account_owner_authority(alice: UpdateAccount, authority_type: str, use_account_update2: bool) -> None:
    """
    Test cases 1, 2 and 3 from issue: https://gitlab.syncad.com/hive/hive/-/issues/519 and
    https://gitlab.syncad.com/hive/hive/-/issues/520
    """
    alice.use_authority(authority_type)
    new_auth = alice.generate_new_authority()
    if authority_type != "owner":
        with pytest.raises(ErrorInResponseError) as exception:
            alice.update_account(owner=new_auth, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_unchanged()
        error_response = exception.value.error
        assert "Missing Owner Authority" in error_response
    else:
        transaction = alice.update_account(owner=new_auth, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_reduced(transaction)
        alice.assert_account_details_were_changed(new_owner=new_auth)


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "authority_type",
    ["active", "posting", "owner"],
)
# When parameter use_account_update2 is False test creates account_update operation otherwise - account_update2
@pytest.mark.parametrize("use_account_update2", [(True, False)])
def test_update_account_active_authority(alice: UpdateAccount, authority_type: str, use_account_update2: bool) -> None:
    """
    Test cases 4,5 and 6 from issue: https://gitlab.syncad.com/hive/hive/-/issues/519 and
    https://gitlab.syncad.com/hive/hive/-/issues/520
    """
    alice.use_authority(authority_type)
    new_auth = alice.generate_new_authority()
    if authority_type == "posting" or authority_type == "owner":
        with pytest.raises(ErrorInResponseError) as exception:
            alice.update_account(active=new_auth, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_unchanged()
        error_response = exception.value.error
        assert "Missing Active Authority" in error_response
    else:
        transaction = alice.update_account(active=new_auth, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_reduced(transaction)
        alice.assert_account_details_were_changed(new_active=new_auth)


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "authority_type",
    ["active", "posting", "owner"],
)
# When parameter use_account_update2 is False test creates account_update operation otherwise - account_update2
@pytest.mark.parametrize("use_account_update2", [(True, False)])
def test_update_account_posting_authority(alice: UpdateAccount, authority_type: str, use_account_update2: bool) -> None:
    """
    Test cases 7,8 and 9 from issue: https://gitlab.syncad.com/hive/hive/-/issues/519 and
    https://gitlab.syncad.com/hive/hive/-/issues/520
    """
    alice.use_authority(authority_type)
    new_auth = alice.generate_new_authority()
    if authority_type == "posting" or authority_type == "owner":
        with pytest.raises(ErrorInResponseError) as exception:
            alice.update_account(posting=new_auth, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_unchanged()
        error_response = exception.value.error
        assert "Missing Active Authority" in error_response
    else:
        transaction = alice.update_account(posting=new_auth, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_reduced(transaction)
        alice.assert_account_details_were_changed(new_posting=new_auth)


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "authority_type",
    ["active", "posting", "owner"],
)
# When parameter use_account_update2 is False test creates account_update operation otherwise - account_update2
@pytest.mark.parametrize("use_account_update2", [(True, False)])
def test_update_account_memo_key(alice: UpdateAccount, authority_type: str, use_account_update2: bool) -> None:
    """
    Test cases 10, 11 and 12 from issue: https://gitlab.syncad.com/hive/hive/-/issues/519 and
    https://gitlab.syncad.com/hive/hive/-/issues/520
    """
    alice.use_authority(authority_type)
    new_key = alice.generate_new_key()
    if authority_type == "posting" or authority_type == "owner":
        with pytest.raises(ErrorInResponseError) as exception:
            alice.update_account(memo_key=new_key, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_unchanged()
        error_response = exception.value.error
        assert "Missing Active Authority" in error_response
    else:
        transaction = alice.update_account(memo_key=new_key, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_reduced(transaction)
        alice.assert_account_details_were_changed(new_memo=new_key)


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "authority_type",
    ["active", "posting", "owner"],
)
# When parameter use_account_update2 is False test creates account_update operation otherwise - account_update2
@pytest.mark.parametrize("use_account_update2", [(True, False)])
def test_update_json_metadata(alice: UpdateAccount, authority_type: str, use_account_update2: bool) -> None:
    """
    Test cases 13, 14 and 15 from issue: https://gitlab.syncad.com/hive/hive/-/issues/519,
    https://gitlab.syncad.com/hive/hive/-/issues/520
    """
    alice.use_authority(authority_type)
    new_json_meta = '{"foo": "bar"}'
    if authority_type == "posting" or authority_type == "owner":
        with pytest.raises(ErrorInResponseError) as exception:
            alice.update_account(json_metadata=new_json_meta, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_unchanged()
        error_response = exception.value.error
        assert "Missing Active Authority" in error_response
    else:
        transaction = alice.update_account(json_metadata=new_json_meta, use_account_update2=use_account_update2)
        alice.assert_if_rc_current_mana_was_reduced(transaction)
        alice.assert_account_details_were_changed(new_json_meta=new_json_meta)


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "authority_type",
    ["active", "posting", "owner"],
)
def test_update_posting_json_metadata(alice: UpdateAccount, authority_type: str) -> None:
    """
    Test cases 16, 17 and 18 from issue: https://gitlab.syncad.com/hive/hive/-/issues/520
    """
    alice.use_authority(authority_type)
    new_posting_json_meta = '{"foo": "bar"}'
    if authority_type == "active" or authority_type == "owner":
        with pytest.raises(ErrorInResponseError) as exception:
            alice.update_account(posting_json_metadata=new_posting_json_meta, use_account_update2=True)
        alice.assert_if_rc_current_mana_was_unchanged()
        error_response = exception.value.error
        assert "Missing Posting Authority" in error_response
    else:
        transaction = alice.update_account(posting_json_metadata=new_posting_json_meta, use_account_update2=True)
        alice.assert_if_rc_current_mana_was_reduced(transaction)
        alice.assert_account_details_were_changed(new_posting_json_meta=new_posting_json_meta)


@pytest.mark.testnet()
# When parameter use_account_update2 is False test creates account_update operation otherwise - account_update2
@pytest.mark.parametrize(
    ("use_account_update2", "posting_json_meta", "assert_posting_json"),
    [
        # test case 19 from issue: https://gitlab.syncad.com/hive/hive/-/issues/520
        (True, {"posting_json_metadata": '{"key": "value"}'}, {"new_posting_json_meta": '{"key": "value"}'}),
        # test case 16 from issue https://gitlab.syncad.com/hive/hive/-/issues/519
        (False, {}, {}),
    ],
)
def test_update_all_account_parameters_using_owner_authority(
    alice: UpdateAccount, use_account_update2: bool, posting_json_meta: dict, assert_posting_json: dict
) -> None:
    alice.use_authority("owner")
    new_json_meta = '{"foo": "bar"}'
    new_owner, new_active, new_posting, new_memo = (
        alice.generate_new_authority(),
        alice.generate_new_authority(),
        alice.generate_new_authority(),
        alice.generate_new_key(),
    )

    transaction = alice.update_account(
        owner=new_owner,
        active=new_active,
        posting=new_posting,
        memo_key=new_memo,
        json_metadata=new_json_meta,
        use_account_update2=use_account_update2,
        **posting_json_meta,
    )

    alice.assert_if_rc_current_mana_was_reduced(transaction)
    alice.assert_account_details_were_changed(
        new_owner=new_owner,
        new_active=new_active,
        new_posting=new_posting,
        new_memo=new_memo,
        new_json_meta=new_json_meta,
        **assert_posting_json,
    )


@pytest.mark.testnet()
# When parameter use_account_update2 is False test creates account_update operation otherwise - account_update2
@pytest.mark.parametrize(
    ("use_account_update2", "posting_json_meta", "assert_posting_json"),
    [
        # test case 20 from issue: https://gitlab.syncad.com/hive/hive/-/issues/520
        (True, {"posting_json_metadata": '{"key": "value"}'}, {"new_posting_json_meta": '{"key": "value"}'}),
        # test case 17 from issue https://gitlab.syncad.com/hive/hive/-/issues/519
        (False, {}, {}),
    ],
)
def test_update_all_account_parameters_except_owner_key_using_active_authority(
    alice: UpdateAccount, use_account_update2: bool, posting_json_meta: dict, assert_posting_json: dict
) -> None:
    alice.use_authority("active")
    new_json_meta = '{"foo": "bar"}'
    new_active, new_posting, new_memo = (
        alice.generate_new_authority(),
        alice.generate_new_authority(),
        alice.generate_new_key(),
    )
    transaction = alice.update_account(
        active=new_active,
        posting=new_posting,
        memo_key=new_memo,
        json_metadata=new_json_meta,
        use_account_update2=use_account_update2,
        **posting_json_meta,
    )
    real_rc_before = alice.rc_manabar.calculate_current_value(
        get_transaction_timestamp(alice.node, transaction) - tt.Time.seconds(3)
    )
    alice.rc_manabar.update()
    real_rc_after = alice.rc_manabar.calculate_current_value(
        get_transaction_timestamp(alice.node, transaction) - tt.Time.seconds(3)
    )
    assert (
        real_rc_before == real_rc_after + transaction["rc_cost"]
    ), "RC wasn't decreased after changing all account details except owner key."
    alice.assert_account_details_were_changed(
        new_active=new_active,
        new_posting=new_posting,
        new_memo=new_memo,
        new_json_meta=new_json_meta,
        **assert_posting_json,
    )


@pytest.mark.testnet()
@pytest.mark.parametrize(
    "iterations",
    [
        2,  # Change owner authority 2 times in less than HIVE_OWNER_UPDATE_LIMIT
        3,  # Change owner authority 3 times in less than HIVE_OWNER_UPDATE_LIMIT
    ],
)
# When parameter use_account_update2 is False test creates account_update operation otherwise - account_update2
@pytest.mark.parametrize(
    "use_account_update2",
    [
        True,  # Test case 21, 22 from issue: https://gitlab.syncad.com/hive/hive/-/issues/520
        False,  # Test case 18, 19 from issue: https://gitlab.syncad.com/hive/hive/-/issues/519
    ],
)
def test_update_owner_authority_two_and_three_times_within_one_hour(
    alice: UpdateAccount, iterations: int, use_account_update2: bool
) -> None:
    alice.use_authority("owner")
    current_key = alice.get_current_key("owner")
    for iteration in range(2, 2 + iterations):
        new_auth = {"account_auths": [], "key_auths": [(current_key, iteration)], "weight_threshold": iteration}
        if iteration == 4:
            with pytest.raises(ErrorInResponseError) as exception:
                alice.update_account(owner=new_auth, use_account_update2=use_account_update2)
            alice.assert_if_rc_current_mana_was_unchanged()
            error_response = exception.value.error
            assert "Owner authority can only be updated twice an hour" in error_response
        else:
            transaction = alice.update_account(owner=new_auth, use_account_update2=use_account_update2)
            alice.assert_if_rc_current_mana_was_reduced(transaction)
            alice.assert_account_details_were_changed(new_owner=new_auth)
