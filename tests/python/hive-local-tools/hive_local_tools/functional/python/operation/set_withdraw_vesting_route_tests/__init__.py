from __future__ import annotations

from typing import TYPE_CHECKING

import test_tools as tt
from hive_local_tools.constants import HIVE_100_PERCENT
from hive_local_tools.functional.python.operation import Operation, get_transaction_timestamp

if TYPE_CHECKING:
    from datetime import datetime


def calculate_asset_percentage(asset, percent_in_hive) -> tt.Asset.Test | tt.Asset.Vest:
    asset_multiplier = percent_in_hive / HIVE_100_PERCENT

    if isinstance(asset, tt.Asset.TestT):
        return tt.Asset.Test(asset.as_float() * asset_multiplier)
    if isinstance(asset, tt.Asset.VestT):
        return tt.Asset.Vest(asset.as_float() * asset_multiplier)
    return None


class SetWithdrawVestingRoute(Operation):
    def __init__(self, node, wallet, from_, to, percent, auto_vest) -> None:
        super().__init__(node, wallet)
        self._from = from_
        self._to = to
        self._percent = percent
        self._auto_vest = auto_vest
        self._transaction = self._wallet.api.set_withdraw_vesting_route(
            self._from, self._to, self._percent, self._auto_vest
        )
        self._timestamp = get_transaction_timestamp(node, self._transaction)
        self._rc_cost = self._transaction["rc_cost"]

    @property
    def timestamp(self) -> datetime:
        return self._timestamp

    @property
    def rc_cost(self) -> int:
        return self._rc_cost


class OrderedRoute:
    def __init__(self, route) -> None:
        self.from_account = route.from_account
        self.to_account = route.to_account
        self.percent = route.percent
        self.auto_vest = route.auto_vest


class VestingRouteParameters:
    def __init__(self, receiver, percent, auto_vest) -> None:
        self.receiver: str = receiver
        self.percent: int = percent
        self.auto_vest: bool = auto_vest


def get_ordered_withdraw_routes(node) -> dict:
    return {
        route.to_account: OrderedRoute(route)
        for route in node.api.database.list_withdraw_vesting_routes(
            start=["", ""], limit=100, order="by_withdraw_route"
        ).routes
    }
