from jsonschema.exceptions import ValidationError
import pytest

from schemas.predefined import *


@pytest.mark.parametrize(
    'schema, instance', [
        # AssetHbd
        (AssetHbd(), {
            'amount': '100',
            'precision': 3,
            'nai': '@@000000013'
        }),

        # AssetHive
        (AssetHive(), {
            'amount': '100',
            'precision': 3,
            'nai': '@@000000021'
        }),

        # AssetVests
        (AssetVests(), {
            'amount': '100',
            'precision': 6,
            'nai': '@@000000037'
        }),

        # Authority
        (Authority(), {
            'weight_threshold': 1,
            'account_auths': [['22', 1]],
            'key_auths': [
                [
                    'STM7AwB4maYkySTZZbx3mtdTaxsKTYyJxhmUZVK9wd53t2qXCvxmB',
                    1
                ]
            ]
        }),

        # Key
        (Key(), 'STM7U2ecB3gEwfrLMQtfVkCN8z3kPmXtDH3HSmLgrbsFpV6UXEwKE'),
        (Key(), 'TST7AwB4maYkySTZZbx3mtdTaxsKTYyJxhmUZVK9wd53t2qXCvxmB'),

        # Manabar
        (Manabar(), {
            'current_mana': '58925267722823',
            'last_update_time': 1646317446
        }),

        #  Proposal
        (Proposal(), {
            'id': 0,
            'proposal_id': 0,
            'creator': 'alice',
            'receiver': 'bob',
            'start_date': '2019-07-01T00:00:00',
            'end_date': '2019-08-01T23:59:59',
            'daily_pay': {
                'amount': '4800000',
                'precision': 3,
                'nai': '@@000000013'
            },
            'subject': 'My Proposal',
            'permlink': 'creator-proposal-permlink',
            'total_votes': '77351826710',
            'status': 'active'
        }),

        #  Version
        (Version(), '0.0.0')
    ]
)
def test_validation_of_correct_type(schema, instance):
    schema.validate(instance)


@pytest.mark.parametrize(
    'schema, instance', [
        # AssetHbd
        (AssetHbd(), {
            'amount': '100',
            'precision': 3,
            'nai': 'wrong-nai'
        }),

        # AssetHive
        (AssetHive(), {
            'amount': 3.141593,  # wrong type of 'amount'
            'precision': 3,
            'nai': '@@000000021'
        }),

        # AssetVests
        (AssetVests(), {
            'amount': '100',
            'precision': 7,  # correct 'precision' value == 6
            'nai': '@@000000037'
        }),

        # Authority
        (Authority(), {
            'weight_threshold': 1,
            'account_auths': [['22', 1]],
            'key_auths': [
                [
                    1,
                    'STM7AwB4maYkySTZZbx3mtdTaxsKTYyJxhmUZVK9wd53t2qXCvxmB'
                ]  # key_auths: wrong array order, should be key, int.
            ]
        }),

        # Key
        (Key(), 'PPP7U2ecB3gEwfrLMQtfVkCN8z3kPmXtDH3HSmLgrbsFpV6UXEwKE'),  # Bad key prefix
        (Key(), 'TST7AwB4maYkySTZZbx3mtdTaxsKTYyJxhmUZ....../////?????'),  # invalid characters
        (Key(), 'TST7AwB4maYkySTZZbx3mtdTaxsKTYyJxhmUZVK9wd5'),  # not enough characters (the amount required is 50)

        #  Verison
        (Version(), '0.0.a')
    ]
)
def test_validation_of_incorrect_type(schema, instance):
    with pytest.raises(ValidationError):
        schema.validate(instance)
