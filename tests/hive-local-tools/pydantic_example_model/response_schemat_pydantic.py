from __future__ import annotations
from pydantic import BaseModel, PositiveInt, ValidationError, Json
from pydantic.generics import GenericModel
import requests
from typing import Generic, TypeVar, Any, Type

from fundamental_schemas_pydantic import (Authority,
                                          Manabar,
                                          AssetHive,
                                          AssetHbd,
                                          AssetVests,
                                          DelayedVotes,
                                          RegexName,
                                          RegexKey,
                                          HiveDateTime)


T = TypeVar("T")


class HiveResult(GenericModel, Generic[T]):
    """
    Response from HIVE includes fields id, jsonrpc and result. We just need to validate result field.
    To analise just result field use factory method from this class. First argument is pydantic model of response
    to validate and second is unpacked response. Remember that result field is list, so you probably need to use format
    like this: list[PydanticResponseModel].
    """
    id: Any
    jsonrpc: str
    result: T

    @staticmethod
    def factory(t: Type[T], **kwargs) -> HiveResult[T]:
        return HiveResult[t](**kwargs)


class AccountItem(BaseModel):
    id: PositiveInt
    name: RegexName
    owner: Authority
    active: Authority
    posting: Authority
    memo_key: RegexKey
    json_metadata: str | Json
    posting_json_metadata: str | Json
    proxy: str
    last_owner_update: HiveDateTime
    last_account_update: HiveDateTime
    created: HiveDateTime
    mined: bool
    recovery_account: str
    last_account_recovery: HiveDateTime
    reset_account: str
    comment_count: int
    lifetime_vote_count: int
    post_count: int
    can_vote: bool
    voting_manabar: Manabar
    downvote_manabar: Manabar
    balance: AssetHive
    savings_balance: AssetHive
    hbd_balance: AssetHbd
    hbd_seconds: int
    hbd_seconds_last_update: HiveDateTime
    hbd_last_interest_payment: HiveDateTime
    savings_hbd_balance: AssetHbd
    savings_hbd_seconds: int
    savings_hbd_seconds_last_update: HiveDateTime
    savings_hbd_last_interest_payment: HiveDateTime
    savings_withdraw_requests: int
    reward_hbd_balance: AssetHbd
    reward_hive_balance: AssetHive
    reward_vesting_balance: AssetVests
    reward_vesting_hive: AssetHive
    vesting_shares: AssetVests
    delegated_vesting_shares: AssetVests
    received_vesting_shares: AssetVests
    vesting_withdraw_rate: AssetVests
    post_voting_power: AssetVests
    next_vesting_withdrawal: HiveDateTime
    withdrawn: int
    to_withdraw: int
    withdraw_routes: int
    pending_transfers: int
    curation_rewards: int
    posting_rewards: int
    proxied_vsf_votes: list[int]
    witnesses_voted_for: int
    last_post: HiveDateTime
    last_root_post: HiveDateTime
    last_post_edit: HiveDateTime
    last_vote_time: HiveDateTime
    post_bandwidth: int
    pending_claimed_accounts: int
    is_smt: bool
    delayed_votes: list[DelayedVotes]
    governance_vote_expiration_ts: HiveDateTime


class FindAccount(BaseModel):
    accounts: list[AccountItem]


class ListAccount(FindAccount):
    pass


if __name__ == "__main__":
    # responses from HIVE
    headers = {'Content-Type': 'application/json', }
    data = '{"jsonrpc":"2.0", "method":"database_api.find_accounts", "params": {"accounts":["hiveio"]}, "id":1}'
    response_from_hive = requests.post('https://api.hive.blog', headers=headers, data=data)

    response_to_check = response_from_hive.json()

    try:
        find_accounts = HiveResult.factory(FindAccount, **response_to_check)
    except ValidationError as e:
        print(e.json())

    print(find_accounts.schema_json(indent=2))
