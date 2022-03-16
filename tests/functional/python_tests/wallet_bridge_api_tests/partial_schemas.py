from typing import Dict, List, Union, Optional, Any
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


def Seq(*content, required: bool = True, matching: str = 'any', options: Optional[Dict[str, Any]] = None):
    if options is None:
        options = {}
    return {
        'required': required,
        'matching': matching,
        'seq': [*content],
        **options,
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


def Manabar():
    return Map({
        "current_mana": Ext_str_and_int(),
        "last_update_time": Int(),
    })


def Authority():
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
                Str(pattern=r'(^STM[A-zA-Z0-9]{50}$|^TST[A-zA-Z0-9]{50}$)'),
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
