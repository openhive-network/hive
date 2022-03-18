import logging
import re
from typing import Any, Dict, List, Optional, Union

from pykwalify.core import Core
from pykwalify.rule import Rule
import pykwalify.types


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

    for item_index, item_schema_pair in enumerate(zip(sequence, rule.sequence)):
        item, schema = item_schema_pair

        logging.disable()  # To ignore logs from below validation
        core = Core(source_data=item, schema_data=schema.schema_str)
        core.validate(raise_exception=False)
        logging.disable(logging.NOTSET)

        if core.errors:
            errors_description = ""
            for error in core.errors:
                error.path = fr'{path}\{item_index}'
                errors_description += f'\n{error}'

            raise AssertionError(f'Validation failed with following errors: {errors_description}')

    return True


def custom_check(value):
    if type(value) == int:
        return True

    if type(value) == str:
        if re.match(r'^\d+$', value):
            return True

    return False


class IntRuleMetaclass(type):
    def __instancecheck__(self, instance):
        return custom_check(instance)


class IntRule(metaclass=IntRuleMetaclass):
    pass

# TODO: TO nie powinno tu być
pykwalify.types.tt.update({'hive_int': custom_check})
pykwalify.types._types.update({'hive_int': IntRule})


def __create_basic_schema(type_: str, required: bool, options: Dict):
    return {
        'type': type_,
        'required': required,
        **options,
    }


def Int2(required: bool = True, **options):
    schema_int = __create_basic_schema('hive_int', required, options)
    return schema_int


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


def Date(required: bool = True):
    return __create_basic_schema('date', required, options={'format': '%Y-%m-%dT%X'})


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
    return Map({
        'amount': Str(),
        'precision': Int(enum=[3]),
        'nai': Str(pattern='@@000000013'),
    })


def AssetHive():
    return Map({
        'amount': Str(),
        'precision': Int(enum=[3]),
        'nai': Str(pattern='@@000000021')
    })


def AssetVests():
    return Map({
        'amount': Str(),
        'precision': Int(enum=[6]),
        'nai': Str(pattern='@@000000037'),
    })


def Manabar():
    return Map({
        "current_mana": Int(),
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
                Key(),
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


def Version():
    return Str(pattern=r'^(\d+\.)?(\d+\.)?(\*|\d+)$')


def Key():
    return Str(pattern=r'^(?:STM|TST)[A-Za-z0-9]{50}$')


def Seq2(*content, required: bool = True, matching: str = 'any', options: Optional[Dict[str, Any]] = None):
    if options is None:
        options = {}

    common = {
        'required': required,
        'seq': [*content],
        **options,
    }

    if matching == 'unique':
        return {
            'func': 'sequence_matches_rule',
            **common,
        }

    return {
        'matching': matching,
        **common,
    }
