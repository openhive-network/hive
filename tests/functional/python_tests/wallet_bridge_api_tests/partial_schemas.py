from typing import Dict, List, Union
from pykwalify.rule import Rule


import logging
import pykwalify.types
import re

log = logging.getLogger(__name__)


def sequence_matches_rule(sequence: List, rule: Rule, path: str) -> bool:
	log.debug("sequence: %s", sequence)
	log.debug("rule: %s", rule)
	log.debug("path: %s", path)

	if len(sequence) != len(rule.sequence):
		raise AssertionError(
			f'Incorrect number of elements in sequence.\n'
			f'Expected: {[item.type for item in rule.sequence]}\n'
			f'Actual: {sequence}'
		)

	for item, schema in list(zip(sequence, rule.sequence)):
		if not pykwalify.types.tt[schema.type](item):
			raise AssertionError(
				f'Incorrect type.\n'
				f'Expected: {schema.type}\n'
				f'Actual: {item} of type {type(item)}'
			)

	return True


def correct_type_str_or_int(value: Union[str, int], rule_obj: Rule, path: str) -> bool:
	if type(value) == int:
		return True
	if type(value) == str:
		if re.match(r'^\d+$', value):
			return True
	raise AssertionError(
		f'Incorrect type.\n'
		f'Expected: int or str type.\n'
		f'Actual: {value} of type {type(value)}'
	)


def Ext_str_and_int(required: bool = True):
	return {
		'func': 'correct_type_str_or_int',
		'required': required,
		'type': 'text',
	}


def __create_basic_schema(type_: str, required: bool, options: Dict):
	return {
		'type': type_,
		'required': required,
		**options,
	}


def Seq_type_matched(*content, required: bool = True, matching: str = 'any'):
	return {
		'func': 'sequence_matches_rule',
		'required': required,
		'matching': matching,
		'seq': [*content],
	}


def Seq(*content, required: bool = True, matching: str = 'any'):
	return {
		'required': required,
		'matching': matching,
		'seq': [*content],
	}


def Map(content: Dict, required: bool = True, matching: str = 'all'):
	return {
		'required': required,
		'matching': matching,
		'map': content,
	}


def Any(required: bool = True, **options):
	return __create_basic_schema('any', required, options)


def Date():
	return {'type': 'date', 'format': "%Y-%m-%dT%X", 'required': True}


def None_(required: bool = True, **options):
	return __create_basic_schema('none', required, options)


def Text(required: bool = True, **options):
	return __create_basic_schema('text', required, options)


def Bool(required: bool = True, **options):
	return __create_basic_schema('bool', required, options)


def Number(required: bool = True, **options):
	return __create_basic_schema('number', required, options)


def Int(required: bool = True, **options):
	return __create_basic_schema('int', required, options)


def Str(required: bool = True, **options):
	return __create_basic_schema('str', required, options)


def Float(required: bool = True, **options):
	return __create_basic_schema('float', required, options)


def AssetHbd():
	return Map(
		{
			'amount': Str(),
			'precision': Int(enum=[3]),
			'nai': Str(pattern='@@000000013'),
		}

	)


def AssetHive():
	return Map(
		{
			'amount': Str(),
			'precision': Int(enum=[3]),
			'nai': Str(pattern='@@000000021')
		}
	)


def AssetVests():
	return Map(
		{
			'amount': Str(),
			'precision': Int(enum=[6]),
			'nai': Str(pattern='@@000000037'),
		}
	)


def Key():
	return Map({
		'weight_threshold': Int(),
		'account_auths': Seq(
			Seq_type_matched(
				Str(),
				Int(),
			)
		),
		'key_auths': Seq(
			Seq_type_matched(
				Str(),
				Int(),
			)
		),
	})


def Proposal():
	return Map({
		'id': Int(),
		'proposal_id': Int(),
		'creator': Str(),
		'receiver': Str(),
		'start_date': Date(),
		'end_date': Date(),
		'daily_pay': AssetHbd(),
		'subject': Str(),
		'permlink': Str(),
		'total_votes': Int(),
		'status': Str(),
	})


test_response_data = {
	"jsonrpc": "2.0",
	"result": {
		"id": 189415,
		"name": "mahdiyari",
		"owner": {
			"weight_threshold": 1,
			"account_auths": [['22', 1]],
			"key_auths": [
				[
					"STM7AwB4maYkySTZZbx3mtdTaxsKTYyJxhmUZVK9wd53t2qXCvxmB",
					1
				]
			]
		},
		"active": {
			"weight_threshold": 1,
			"account_auths": [],
			"key_auths": [
				[
					"STM8JhZj3W3jYtVsEbU15Sciq4aDTvvcZ9oqY2vaBXagVc4X44StJ",
					1
				]
			]
		},
		"posting": {
			"weight_threshold": 1,
			"account_auths": [
				[
					"leofinance",
					1
				],
				[
					"steemauto",
					1
				]
			],
			"key_auths": [
				[
					"STM7U2ecB3gEwfrLMQtfVkCN8z3kPmXtDH3HSmLgrbsFpV6UXEwKE",
					1
				]
			]
		},
		"memo_key": "STM6dCzKuDCfkbr2HTNLskjtVPKckbLc4xfFDU3MqbLTNyG8zYNpg",
		"json_metadata": "{\"profile\":{\"profile_image\":\"http://s6.uplod.ir/i/00885/b7de9gumvkde.jpg\",\"name\":\"Mahdi Yari\",\"about\":\"Witness, Developer, Founder of SteemAuto and Steemfollower, Crypto Analyser and Chemical & Software Eng. - Please Vote for 'mahdiyari' Witness.\",\"location\":\"STEEM\",\"website\":\"https://steemauto.com\",\"cover_image\":\"http://s6.uplod.ir/i/00893/09bqh9kx9q7n.jpg\",\"dtube_pub\":\"f8uJWzQeTn4m9CC4q8XfHe8rpixKTL5F8MBsSdf8rUxd\"}}",
		"posting_json_metadata": "{\"profile\":{\"profile_image\":\"http://s6.uplod.ir/i/00885/b7de9gumvkde.jpg\",\"name\":\"Mahdi Yari\",\"about\":\"Hive Witness\",\"location\":\"Hive\",\"website\":\"https://hive.vote\",\"cover_image\":\"http://s6.uplod.ir/i/00893/09bqh9kx9q7n.jpg\",\"dtube_pub\":\"f8uJWzQeTn4m9CC4q8XfHe8rpixKTL5F8MBsSdf8rUxd\",\"witness_description\":\"Dedicated to Hive\",\"version\":2}}",
		"proxy": "",
		"previous_owner_update": "2017-06-22T16:31:48",
		"last_owner_update": "2017-06-23T09:16:45",
		"last_account_update": "2021-08-13T15:52:57",
		"created": "2017-06-13T14:24:24",
		"mined": False,
		"recovery_account": "hive-recovery",
		"last_account_recovery": "1970-01-01T00:00:00",
		"reset_account": "null",
		"comment_count": 0,
		"lifetime_vote_count": 0,
		"post_count": 2172,
		"can_vote": True,
		"voting_manabar": {
			"current_mana": "58925267722823",
			"last_update_time": 1646317446
		},
		"downvote_manabar": {
			"current_mana": "18175780751415",
			"last_update_time": 1646317446
		},
		"balance": {
			"amount": "17479520",
			"precision": 3,
			"nai": "@@000000021"
		},
		"savings_balance": {
			"amount": "0",
			"precision": 3,
			"nai": "@@000000021"
		},
		"hbd_balance": {
			"amount": "261659",
			"precision": 3,
			"nai": "@@000000013"
		},
		"hbd_seconds": "4264077138",
		"hbd_seconds_last_update": "2021-06-30T13:57:00",
		"hbd_last_interest_payment": "2021-06-29T02:29:21",
		"savings_hbd_balance": {
			"amount": "0",
			"precision": 3,
			"nai": "@@000000013"
		},
		"savings_hbd_seconds": "133193744832",
		"savings_hbd_seconds_last_update": "2021-08-20T07:08:39",
		"savings_hbd_last_interest_payment": "2021-08-11T09:05:27",
		"savings_withdraw_requests": 0,
		"reward_hbd_balance": {
			"amount": "19",
			"precision": 3,
			"nai": "@@000000013"
		},
		"reward_hive_balance": {
			"amount": "0",
			"precision": 3,
			"nai": "@@000000021"
		},
		"reward_vesting_balance": {
			"amount": "1100914410",
			"precision": 6,
			"nai": "@@000000037"
		},
		"reward_vesting_hive": {
			"amount": "598",
			"precision": 3,
			"nai": "@@000000021"
		},
		"vesting_shares": {
			"amount": "73023693865810",
			"precision": 6,
			"nai": "@@000000037"
		},
		"delegated_vesting_shares": {
			"amount": "320570860148",
			"precision": 6,
			"nai": "@@000000037"
		},
		"received_vesting_shares": {
			"amount": "0",
			"precision": 6,
			"nai": "@@000000037"
		},
		"vesting_withdraw_rate": {
			"amount": "0",
			"precision": 6,
			"nai": "@@000000037"
		},
		"post_voting_power": {
			"amount": "72703123005662",
			"precision": 6,
			"nai": "@@000000037"
		},
		"next_vesting_withdrawal": "1969-12-31T23:59:59",
		"withdrawn": 0,
		"to_withdraw": 0,
		"withdraw_routes": 0,
		"pending_transfers": 0,
		"curation_rewards": 8200190,
		"posting_rewards": 5508724,
		"proxied_vsf_votes": [
			"11355254831846",
			0,
			'11',
			0
		],
		"witnesses_voted_for": 18,
		"last_post": "2022-02-27T23:08:15",
		"last_root_post": "2021-12-31T12:28:51",
		"last_post_edit": "2022-02-27T23:08:15",
		"last_vote_time": "2022-03-03T14:15:12",
		"post_bandwidth": 0,
		"pending_claimed_accounts": 3,
		"open_recurrent_transfers": 0,
		"is_smt": False,
		"delayed_votes": [],
		"governance_vote_expiration_ts": "2022-08-12T18:42:03"
	},
	"id": 1
}
