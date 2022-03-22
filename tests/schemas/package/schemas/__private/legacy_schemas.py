from schemas.predefined import *


__create_custom_schema(name, desc, ???)


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
            Seq(
                Str(),
                Int(),
                matching='strict',
            )
        ),
        'key_auths': Seq(
            Seq(
                Key(),
                Int(),
                matching='strict',
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
