"""Scenarios description: https://gitlab.syncad.com/hive/hive/-/issues/539"""
from __future__ import annotations

from typing import Callable

import pytest

import test_tools as tt
from hive_local_tools import run_for
from hive_local_tools.constants import HIVE_100_PERCENT
from hive_local_tools.functional.python.operation import get_virtual_operations
from hive_local_tools.functional.python.operation.set_withdraw_vesting_route_tests import (
    SetWithdrawVestingRoute,
    VestingRouteParameters,
    calculate_asset_percentage,
    get_ordered_withdraw_routes,
)
from hive_local_tools.functional.python.operation.withdrawe_vesting import (
    PowerDown,
    PowerDownAccount,
)
from schemas.operations.virtual import FillVestingWithdrawOperation

INIT_WITHDRAW_VESTING_ROUTE_PARAMETERS = [
    [VestingRouteParameters(receiver="user-b0", percent=HIVE_100_PERCENT, auto_vest=False)],
    [VestingRouteParameters(receiver="user-b0", percent=HIVE_100_PERCENT, auto_vest=True)],
    [
        VestingRouteParameters(receiver="user-b0", percent=1000, auto_vest=False),
        VestingRouteParameters(receiver="user-b1", percent=2000, auto_vest=True),
        VestingRouteParameters(receiver="user-b2", percent=3000, auto_vest=False),
    ],
    [VestingRouteParameters(receiver=f"user-b{num}", percent=1000, auto_vest=num < 5) for num in range(10)],
    [VestingRouteParameters(receiver=f"user-b{num}", percent=500, auto_vest=num < 5) for num in range(10)],
]


# Parameters for test cases 11-20
MODIFY_WITHDRAW_VESTING_ROUTE_PARAMETERS = [
    [VestingRouteParameters(receiver="user-b0", percent=0, auto_vest=False)],
    [VestingRouteParameters(receiver="user-b0", percent=5000, auto_vest=False)],
    [VestingRouteParameters(receiver="user-b3", percent=4000, auto_vest=True)],
    [VestingRouteParameters(receiver="user-b0", percent=500, auto_vest=True)],
    [VestingRouteParameters(receiver=f"user-b{num}", percent=0, auto_vest=True) for num in range(5)],
]


@pytest.mark.parametrize("vesting_route_init_parameters", INIT_WITHDRAW_VESTING_ROUTE_PARAMETERS)
@run_for("testnet")
def test_set_withdraw_vesting_route_after_first_power_down(
    node: tt.InitNode,
    wallet: tt.Wallet,
    user_a: PowerDownAccount,
    create_receivers: Callable,
    vesting_route_init_parameters: list[VestingRouteParameters],
) -> None:
    """
    User sets the withdraw vesting route when the power down already exists.
    https://gitlab.syncad.com/hive/hive/-/issues/539#test-suite---user-sets-the-withdraw-vesting-route-when-the-power-down-already-exists

    Timeline:
        0d                                7d                                14d                               21d
        │‾‾‾‾‾‾‾‾‾‾‾ Set routes ‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾
    ────■───────────────●─────────────────■─────────────────────────────────■─────────────────────────────────■─────>[t]
        First PD                          Second PD                         Third PD
        withdraw                          withdraw                          withdraw
    """
    users = create_receivers(vesting_route_init_parameters)
    power_down = PowerDown(node, wallet, user_a.name, user_a.vest)

    # restart node before second power down withdraw
    node.wait_for_irreversible_block()
    node.restart(
        time_control=tt.StartTimeControl(
            start_time=power_down.tranche_schedule[0] - tt.Time.minutes(5),
            speed_up_rate=5,
        )
    )
    user_a.rc_manabar.update()

    for user, parameter in zip(users, vesting_route_init_parameters):
        set_vesting_route = SetWithdrawVestingRoute(
            node, wallet, user_a.name, user.name, parameter.percent, parameter.auto_vest
        )
        user_a.rc_manabar.assert_rc_current_mana_is_reduced(
            operation_rc_cost=set_vesting_route.rc_cost,
            operation_timestamp=set_vesting_route.timestamp,
        )
        node.wait_for_irreversible_block()

    assert len(wallet.api.get_withdraw_routes(user_a.name, "outgoing")) == len(
        vesting_route_init_parameters
    ), "Incorrect number of vesting routes created"
    user_a.update_account_info()

    for week in range(1, 10):
        power_down.execute_next_withdraw()
        [user.update_account_info() for user in users]

        user_a.rc_manabar.assert_max_rc_mana_state("reduced")
        user_a.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)

        remaining_power_down_percentage = HIVE_100_PERCENT - sum(
            [parameter.percent for parameter in vesting_route_init_parameters]
        )
        err = "Unexpected number of fill_vesting_withdraw_operation virtual operations."
        if remaining_power_down_percentage:
            assert len(get_virtual_operations(node, FillVestingWithdrawOperation)) == (len(users) + 1) * week, err
            assert user_a.get_hive_balance() == calculate_asset_percentage(
                week * power_down.weekly_hive_income, remaining_power_down_percentage
            ), "Hive balance is not increased by weekly income."
        else:
            assert len(get_virtual_operations(node, FillVestingWithdrawOperation)) == len(users) * week, err

        for user, parameter in zip(users, vesting_route_init_parameters):
            if parameter.auto_vest:
                assert user.get_hive_power() == calculate_asset_percentage(
                    power_down.weekly_vest_reduction * week, parameter.percent
                ), f"HivePower balance for {user} is not increased by weekly income."
            else:
                assert user.get_hive_balance() == calculate_asset_percentage(
                    power_down.weekly_hive_income * week, parameter.percent
                ), f"Hive balance for {user} is not increased by weekly income."

        user_a.update_account_info()


@pytest.mark.parametrize("vesting_route_init_parameters", INIT_WITHDRAW_VESTING_ROUTE_PARAMETERS)
@run_for("testnet")
def test_set_withdraw_vesting_route_when_the_power_down_doesnt_exist(
    node: tt.InitNode,
    wallet: tt.Wallet,
    user_a: PowerDownAccount,
    create_receivers: Callable,
    vesting_route_init_parameters: list[VestingRouteParameters],
) -> None:
    """
    User sets the withdraw vesting route when the power down doesn't exist.
    https://gitlab.syncad.com/hive/hive/-/issues/539#test-suite---user-sets-the-withdraw-vesting-route-when-the-power-down-doesnt-exist

    Timeline:
                        7d                                14d                               21d
                        │‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
    ────────●───────────■─────────────────────────────────■─────────────────────────────────■───────────────────────>[t]
       Set withdraw     First PD                          Second PD
       routes           withdraw                          withdraw
    """
    users = create_receivers(vesting_route_init_parameters)

    error_message = "User should have no active power down."
    assert user_a.withdrawn == 0, error_message
    assert user_a.to_withdraw == 0, error_message

    for user, parameter in zip(users, vesting_route_init_parameters):
        set_vesting_route = SetWithdrawVestingRoute(
            node, wallet, user_a.name, user.name, parameter.percent, parameter.auto_vest
        )
        user_a.rc_manabar.assert_rc_current_mana_is_reduced(
            operation_rc_cost=set_vesting_route.rc_cost,
            operation_timestamp=set_vesting_route.timestamp,
        )

    assert len(wallet.api.get_withdraw_routes(user_a.name, "outgoing")) == len(
        vesting_route_init_parameters
    ), "Incorrect number of vesting routes created"

    #  user_a creates power down.
    power_down = PowerDown(node, wallet, user_a.name, user_a.vest)
    node.wait_for_irreversible_block()
    user_a.update_account_info()

    for week in range(1, 10):
        power_down.execute_next_withdraw()
        [user.update_account_info() for user in users]

        user_a.rc_manabar.assert_max_rc_mana_state("reduced")
        user_a.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)

        remaining_power_down_percentage = HIVE_100_PERCENT - sum(
            [parameter.percent for parameter in vesting_route_init_parameters]
        )
        if remaining_power_down_percentage:
            assert (
                len(get_virtual_operations(node, FillVestingWithdrawOperation)) == (len(users) + 1) * week
            ), "Unexpected number of fill_vesting_withdraw_operation virtual operations."
            assert user_a.get_hive_balance() == calculate_asset_percentage(
                week * power_down.weekly_hive_income, remaining_power_down_percentage
            ), "Hive balance is not increased by weekly income."
        else:
            assert (
                len(get_virtual_operations(node, FillVestingWithdrawOperation)) == len(users) * week
            ), "Unexpected number of fill_vesting_withdraw_operation virtual operations."

        for user, parameter in zip(users, vesting_route_init_parameters):
            if parameter.auto_vest:
                assert user.get_hive_power() == calculate_asset_percentage(
                    power_down.weekly_vest_reduction * week, parameter.percent
                ), f"HivePower balance for {user} is not increased by weekly income."
            else:
                assert user.get_hive_balance() == calculate_asset_percentage(
                    power_down.weekly_hive_income * week, parameter.percent
                ), f"Hive balance for {user} is not increased by weekly income."

        user_a.update_account_info()


@run_for("testnet")
@pytest.mark.parametrize(
    ("vesting_route_init_parameters", "vesting_route_modify_parameters"),
    zip(INIT_WITHDRAW_VESTING_ROUTE_PARAMETERS, MODIFY_WITHDRAW_VESTING_ROUTE_PARAMETERS),
)
def test_set_withdraw_vesting_route_after_first_withdraw_and_modify_it_before_third_withdraw(
    node: tt.InitNode,
    wallet: tt.Wallet,
    user_a: PowerDownAccount,
    create_receivers: Callable,
    vesting_route_init_parameters: list[VestingRouteParameters],
    vesting_route_modify_parameters: list[VestingRouteParameters],
) -> None:
    """
    User updates settings of the withdraw vesting route after the second withdrawal.
    https://gitlab.syncad.com/hive/hive/-/issues/539#test-suite---user-sets-the-withdraw-vesting-route-when-the-power-down-doesnt-exist

    Timeline:
        0d                                7d                                14d                               21d
        │‾‾‾‾‾‾‾‾‾‾‾ Set routes ‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾ Modify routes ‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾
    ────■───────────────●─────────────────■───────────────●─────────────────■─────────────────────────────────■─────>[t]
        First PD                          Second PD                         Third PD
        withdraw                          withdraw                          withdraw
    """
    users = create_receivers(vesting_route_init_parameters + vesting_route_modify_parameters)
    power_down = PowerDown(node, wallet, user_a.name, user_a.vest)

    # restart node before second power down withdraw
    node.wait_for_irreversible_block()
    node.restart(
        time_control=tt.StartTimeControl(
            start_time=power_down.tranche_schedule[0] - tt.Time.minutes(5),
            speed_up_rate=5,
        )
    )
    user_a.rc_manabar.update()

    for user, parameter in zip(users, vesting_route_init_parameters):
        set_vesting_route = SetWithdrawVestingRoute(
            node, wallet, user_a.name, user.name, parameter.percent, parameter.auto_vest
        )
        user_a.rc_manabar.assert_rc_current_mana_is_reduced(
            operation_rc_cost=set_vesting_route.rc_cost, operation_timestamp=set_vesting_route.timestamp
        )
    node.wait_for_irreversible_block()

    assert len(wallet.api.get_withdraw_routes(user_a.name, "outgoing")) == len(
        vesting_route_init_parameters
    ), "Incorrect number of vesting routes created"
    user_a.update_account_info()

    power_down.execute_next_withdraw()
    vops_after_first_withdraw = len(get_virtual_operations(node, FillVestingWithdrawOperation))
    hive_balance_after_first_withdraw = user_a.get_hive_balance()
    [user.update_account_info() for user in users]

    user_a.rc_manabar.assert_max_rc_mana_state("reduced")
    user_a.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)
    user_a.rc_manabar.update()

    for modify_vesting_route in vesting_route_modify_parameters:
        modify = SetWithdrawVestingRoute(
            node,
            wallet,
            user_a.name,
            modify_vesting_route.receiver,
            modify_vesting_route.percent,
            modify_vesting_route.auto_vest,
        )
        user_a.rc_manabar.assert_rc_current_mana_is_reduced(
            operation_rc_cost=modify.rc_cost, operation_timestamp=modify.timestamp
        )

    node.wait_for_irreversible_block()
    user_a.update_account_info()
    all_ordered_routes = get_ordered_withdraw_routes(node)

    for week in range(1, 10):
        power_down.execute_next_withdraw()

        user_a.rc_manabar.assert_max_rc_mana_state("reduced")
        user_a.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)

        if sum([route.percent for route in all_ordered_routes.values()]) < HIVE_100_PERCENT:
            assert (
                len(get_virtual_operations(node, FillVestingWithdrawOperation))
                == vops_after_first_withdraw + (1 + len(all_ordered_routes)) * week
            ), "Unexpected number of fill_vesting_withdraw_operation virtual operations."
            assert user_a.get_hive_balance() == hive_balance_after_first_withdraw + calculate_asset_percentage(
                week * power_down.weekly_hive_income,
                HIVE_100_PERCENT - sum([route.percent for route in all_ordered_routes.values()]),
            ), "Hive balance is not increased by weekly income."
        else:
            assert (
                len(get_virtual_operations(node, FillVestingWithdrawOperation))
                == vops_after_first_withdraw + len(all_ordered_routes) * week
            ), "Unexpected number of fill_vesting_withdraw_operation virtual operations."

        for user in users:
            if user.name not in all_ordered_routes:
                user.assert_hive_power_is_unchanged()
                user.assert_hive_balance_is_unchanged()
            elif all_ordered_routes[user.name].auto_vest:
                assert user.get_hive_power() == user.vest + calculate_asset_percentage(
                    power_down.weekly_vest_reduction,
                    all_ordered_routes[user.name].percent if user.name in all_ordered_routes else 0,
                ), f"HivePower balance for {user} is not increased by weekly income."
            else:
                assert user.get_hive_balance() == user.hive + calculate_asset_percentage(
                    power_down.weekly_hive_income,
                    all_ordered_routes[user.name].percent if user.name in all_ordered_routes else 0,
                ), f"Hive balance for {user} is not increased by weekly income."

        [user.update_account_info() for user in [*users, user_a]]
        node.wait_for_irreversible_block()


@pytest.mark.parametrize(
    ("vesting_route_init_parameters", "vesting_route_modify_parameters"),
    zip(INIT_WITHDRAW_VESTING_ROUTE_PARAMETERS, MODIFY_WITHDRAW_VESTING_ROUTE_PARAMETERS),
)
def test_set_withdraw_vesting_route_before_the_first_withdraw_and_modify_it_before_second_withdraw(
    node: tt.InitNode,
    wallet: tt.Wallet,
    user_a: PowerDownAccount,
    create_receivers: Callable,
    vesting_route_init_parameters: list[VestingRouteParameters],
    vesting_route_modify_parameters: list[VestingRouteParameters],
) -> None:
    """
    User updates settings of the withdraw vesting route after the first withdrawal.
    https://gitlab.syncad.com/hive/hive/-/issues/539#test-suite---user-updates-settings-of-the-withdraw-vesting-route-after-the-first-withdrawal

    Timeline:

      0d                                7d                                14d                               21d
      │‾‾‾‾‾‾ Set withdraw routes ‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾ Modify route ‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾│‾‾‾‾‾‾‾
    ──■───────────────●─────────────────■───────────────●─────────────────■─────────────────────────────────■───────>[t]
    Set PD                              First PD                          Second PD                         Third PD
                                        withdraw                          withdraw                          withdraw
    """
    users = create_receivers(vesting_route_init_parameters + vesting_route_modify_parameters)
    power_down = PowerDown(node, wallet, user_a.name, user_a.vest)

    # restart node before second power down withdraw
    node.wait_for_irreversible_block()
    node.restart(
        time_control=tt.StartTimeControl(
            start_time=power_down.tranche_schedule[0] - tt.Time.minutes(5),
            speed_up_rate=5,
        )
    )
    user_a.rc_manabar.update()

    for user, parameter in zip(users, vesting_route_init_parameters):
        set_vesting_route = SetWithdrawVestingRoute(
            node, wallet, user_a.name, user.name, parameter.percent, parameter.auto_vest
        )
        user_a.rc_manabar.assert_rc_current_mana_is_reduced(
            operation_rc_cost=set_vesting_route.rc_cost, operation_timestamp=set_vesting_route.timestamp
        )

    assert len(wallet.api.get_withdraw_routes(user_a.name, "outgoing")) == len(
        vesting_route_init_parameters
    ), "Incorrect number of vesting routes created"
    node.wait_for_irreversible_block()
    power_down.execute_next_withdraw()

    vops_after_first_withdraw = len(get_virtual_operations(node, FillVestingWithdrawOperation))
    hive_balance_after_first_withdraw = user_a.get_hive_balance()
    [user.update_account_info() for user in users]

    user_a.rc_manabar.update()

    for modify_vesting_route in vesting_route_modify_parameters:
        modify = SetWithdrawVestingRoute(
            node,
            wallet,
            user_a.name,
            modify_vesting_route.receiver,
            modify_vesting_route.percent,
            modify_vesting_route.auto_vest,
        )
        user_a.rc_manabar.assert_rc_current_mana_is_reduced(
            operation_rc_cost=modify.rc_cost, operation_timestamp=modify.timestamp
        )

    node.wait_for_irreversible_block()
    user_a.update_account_info()
    all_ordered_routes = get_ordered_withdraw_routes(node)

    for week in range(1, 10):
        power_down.execute_next_withdraw()

        user_a.rc_manabar.assert_max_rc_mana_state("reduced")
        user_a.assert_hive_power_balance_is_reduced_by_weekly_amount(power_down.weekly_vest_reduction)

        if sum([route.percent for route in all_ordered_routes.values()]) < HIVE_100_PERCENT:
            assert (
                len(get_virtual_operations(node, FillVestingWithdrawOperation))
                == vops_after_first_withdraw + (1 + len(all_ordered_routes)) * week
            ), "Unexpected number of fill_vesting_withdraw_operation virtual operations."
            assert user_a.get_hive_balance() == hive_balance_after_first_withdraw + calculate_asset_percentage(
                week * power_down.weekly_hive_income,
                HIVE_100_PERCENT - sum([route.percent for route in all_ordered_routes.values()]),
            ), "Hive balance is not increased by weekly income."
        else:
            assert (
                len(get_virtual_operations(node, FillVestingWithdrawOperation))
                == vops_after_first_withdraw + len(all_ordered_routes) * week
            ), "Unexpected number of fill_vesting_withdraw_operation virtual operations."

        for user in users:
            if user.name not in all_ordered_routes:
                user.assert_hive_power_is_unchanged()
                user.assert_hive_balance_is_unchanged()
            elif all_ordered_routes[user.name].auto_vest:
                assert user.get_hive_power() == user.vest + calculate_asset_percentage(
                    power_down.weekly_vest_reduction,
                    all_ordered_routes[user.name].percent if user.name in all_ordered_routes else 0,
                ), f"HivePower balance for {user} is not increased by weekly income."
            else:
                assert user.get_hive_balance() == user.hive + calculate_asset_percentage(
                    power_down.weekly_hive_income,
                    all_ordered_routes[user.name].percent if user.name in all_ordered_routes else 0,
                ), f"Hive balance for {user} is not increased by weekly income."

        [user.update_account_info() for user in [*users, user_a]]
        node.wait_for_irreversible_block()
