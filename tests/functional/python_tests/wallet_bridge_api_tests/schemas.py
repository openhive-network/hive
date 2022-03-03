from partial_schemas import *


find_rc_accounts = Seq(
    Map({
        'account': Str(),
        'rc_manabar': Map({
            'current_mana': Text(),
            'last_update_time': Int(),
        }),
        'max_rc_creation_adjustment': AssetVests(),
        'max_rc': Int(),
        'delegated_rc': Int(),
        'received_delegated_rc': Int(),
    })
)

list_rc_accounts = Seq(
    Map({
        'account': Str(),
        'rc_manabar': Map({
            'current_mana': Text(),
            'last_update_time': Int(),
        }),
        'max_rc_creation_adjustment': AssetVests(),
        'max_rc': Int(),
        'delegated_rc': Int(),
        'received_delegated_rc': Int(),
    })
)

list_rc_direct_delegations = Seq(
    Map({
        'from': Str(),
        'to': Str(),
        'delegated_rc': Int()
    })
)

get_version = Map({
    'blockchain_version': Str(pattern='^(\d+\.)?(\d+\.)?(\*|\d+)$'),
    'hive_revision': Str(),
    'fc_revision': Str(),
    'chain_id': Str(),
})

get_hardfork_version = Str(pattern='^(\d+\.)?(\d+\.)?(\*|\d+)$')

get_active_witnesses = Map({
    'witnesses': Seq(
        Str(),
    )
})

get_account = Map({
    'id': Int(),
    'name': Str(),
    'owner': Key(),
    'active': Key(),
    'posting': Key(),
    'memo_key': Str(),
    'json_metadata': Str(),
    'posting_json_metadata': Str(),
    'proxy': Str(),
    'previous_owner_update': Date(),
    'last_owner_update': Date(),
    'last_account_update': Date(),
    'created': Date(),
    'mined': Bool(),
    "recovery_account": Str(),
    "last_account_recovery": Date(),
    "reset_account": Str(),
    "comment_count": Int(),
    "lifetime_vote_count": Int(),
    "post_count": Int(),
    "can_vote": Bool(),
    "voting_manabar": Map({
        "current_mana": Ext_str_and_int(),
        "last_update_time": Int(),
    }),
    "downvote_manabar": Map({
        "current_mana": Ext_str_and_int(),
        "last_update_time": Int(),
    }),
    "balance": AssetHive(),
    "savings_balance": AssetHive(),
    "hbd_balance": AssetHbd(),
    "hbd_seconds": Str(),
    "hbd_seconds_last_update": Date(),
    "hbd_last_interest_payment": Date(),
    "savings_hbd_balance": AssetHbd(),
    "savings_hbd_seconds": Str(),
    "savings_hbd_seconds_last_update": Date(),
    "savings_hbd_last_interest_payment": Date(),
    "savings_withdraw_requests": Int(),
    "reward_hbd_balance": AssetHbd(),
    "reward_hive_balance": AssetHive(),
    "reward_vesting_balance": AssetVests(),
    "reward_vesting_hive": AssetHive(),
    "vesting_shares": AssetVests(),
    "delegated_vesting_shares": AssetVests(),
    "received_vesting_shares": AssetVests(),
    "vesting_withdraw_rate": AssetVests(),
    "post_voting_power": AssetVests(),
    "next_vesting_withdrawal": Date(),
    "withdrawn": Int(),
    "to_withdraw": Int(),
    "withdraw_routes": Int(),
    "pending_transfers": Int(),
    "curation_rewards": Int(),
    "posting_rewards": Int(),
    "proxied_vsf_votes": Seq(
        Ext_str_and_int()
    ),
    "witnesses_voted_for": Int(),
    "last_post": Date(),
    "last_root_post": Date(),
    "last_post_edit": Date(),
    "last_vote_time": Date(),
    "post_bandwidth": Int(),
    "pending_claimed_accounts": Int(),
    "open_recurrent_transfers": Int(),
    "is_smt": Bool(),
    "delayed_votes": Seq(Map({
        'time': Date(),
        'val': Int(),
        }
    )),
    "governance_vote_expiration_ts": Date(),
})

example = Ext_str_and_int()

