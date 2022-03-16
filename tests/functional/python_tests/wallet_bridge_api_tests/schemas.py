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
    'owner': Authority(),
    'active': Authority(),
    'posting': Authority(),
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
    "voting_manabar": Manabar(),
    "downvote_manabar": Manabar(),
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

list_proposals = (
    Map({
        'proposals': Seq(
            Proposal()
        )
    })
)

list_proposal_votes = Map({
    'proposal_votes': Seq(
        Map({
            'id': Int(),
            'voter': Str(),
            'proposal': Proposal(),
        })
    )
})

find_proposals = Map({
    'proposals': Seq(
        Proposal(),
    )
})

get_witnesses_schedule = Map({
    'id': Int(),
    'current_virtual_time': Str(),
    'next_shuffle_block_num': Int(),
    'current_shuffled_witnesses': Seq(
        Str()
    ),
    'num_scheduled_witnesses': Int(range={'min': 0, 'max': 21}),
    'elected_weight': Int(),
    'timeshare_weight': Int(),
    'miner_weight': Int(),
    'witness_pay_normalization_factor': Int(),
    'median_props': Map({
        'account_creation_fee': AssetHive(),
        'maximum_block_size': Int(),
        'hbd_interest_rate': Int(),
        'account_subsidy_budget': Int(),
        'account_subsidy_decay': Int(),
    }),
    'majority_version': Str(),
    'max_voted_witnesses': Int(),
    'max_miner_witnesses': Int(),
    'max_runner_witnesses': Int(),
    'hardfork_required_witnesses': Int(),
    'account_subsidy_rd': Map({
        'resource_unit': Int(),
        'budget_per_time_unit': Int(),
        'pool_eq': Int(),
        'max_pool_size': Int(),
        'decay_params': Map({
            'decay_per_time_unit': Int(),
            'decay_per_time_unit_denom_shift': Int(),
        }),
        'min_decay': Int(),
    }),
    'account_subsidy_witness_rd': Map({
        'resource_unit': Int(),
        'budget_per_time_unit': Int(),
        'pool_eq': Int(),
        'max_pool_size': Int(),
        'decay_params': Map({
            'decay_per_time_unit': Int(),
            'decay_per_time_unit_denom_shift': Int(),
        }),
        'min_decay': Int(),
    }),
    'min_witness_account_subsidy_decay': Int(),
})

list_witnesses = Map({
    'response': Seq(
        Str(),
    )
})
