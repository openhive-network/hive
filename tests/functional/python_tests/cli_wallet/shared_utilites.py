from test_tools import Account, Asset
from random import randint
from datetime import datetime, timedelta


class funded_account_info:
    def __init__(self):
        self.creator: Account = None
        self.account: Account = None
        self.funded_TESTS: Asset.Test = None
        self.funded_TBD: Asset.Tbd = None
        self.funded_VESTS: Asset.Test = None


def format_datetime(input) -> str:
    return input.strftime('%Y-%m-%dT%H:%M:%S')


def find_proposals_by_creator_name(_creator, _proposal_list):
    proposals = []
    for rs in _proposal_list:
        if rs["creator"] == _creator:
            proposals.append(rs)
    return proposals


def find_proposals_by_voter_name(_voter, _proposal_list):
    proposals = []
    for rs in _proposal_list:
        if rs["voter"] == _voter:
            proposals.append(rs)
    return proposals


class prepared_proposal_data:
    def __init__(self):
        self.post_comment_arguments: dict = None
        self.create_proposal_arguments: dict = None
        self.permlink : str = None
        self.start_date : datetime = None
        self.end_date : datetime = None

class prepared_proposal_data_with_id(prepared_proposal_data):
  def __init__(self, base : prepared_proposal_data, id : int = None):
      super().__init__()
      self.id : int = id
      self.permlink = base.permlink
      self.create_proposal_arguments = base.create_proposal_arguments
      self.post_comment_arguments = base.post_comment_arguments
      self.start_date = base.start_date
      self.end_date = base.end_date

def argument_as_list( item ) -> list:
    return item if isinstance(item, list) else [item]

def get_listing_args(limit: int = 50, order_by: str = "by_creator", order_type: str = "ascending", status: str = "all"):
    return {
        "limit": limit,
        "order_by": order_by,
        "order_type": order_type,
        "status": status
    }


def get_list_proposal_args(start: list, **kwargs):
    result = get_listing_args(**kwargs)
    result["start"] = argument_as_list(start)
    return result


def get_list_proposal_votes_args(start: list, **kwargs):
    if not 'order_by' in kwargs:
        kwargs['order_by'] = 'by_voter_proposal'
    result = get_listing_args(**kwargs)
    result["start"] = argument_as_list(start)
    return result


def prepare_proposal(input: funded_account_info, prefix: str = "test-", author_is_creator : bool = True) -> prepared_proposal_data:
    from hashlib import md5

    creator : Account = input.creator if author_is_creator else input.account
    hash_input = f'{randint(0, 9999)}{prefix}{creator.private_key}{creator.public_key}{creator.name}'
    PERMLINK = md5(hash_input.encode('utf-8')).hexdigest()
    result = prepared_proposal_data()

    result.post_comment_arguments = {
        "author": creator.name,
        "permlink": PERMLINK,
        "parent_author": "",
        "parent_permlink": f"{prefix}post-parent-permlink",
        "title": f"{prefix}post-title",
        "body": f"{prefix}post-body",
        "json": "{}"
    }

    result.start_date = datetime.now() + timedelta(weeks=5)
    result.end_date = datetime.now() + timedelta(weeks=10)

    result.create_proposal_arguments = {
        "creator": creator.name,
        "receiver": input.account.name,
        "start_date": format_datetime(result.start_date),
        "end_date": format_datetime(result.end_date),
        "daily_pay": Asset.Tbd(1),
        "subject": f"{prefix}-create-proposal-subject",
        "permlink": PERMLINK
    }

    result.permlink = PERMLINK
    return result

def print_test_name(fun, *args, **kwargs):
  def print_test_name_impl():#(fun, *args, **kwargs):
    print(f"starting test case: {fun.__name__}")
    return fun(*args, **kwargs)

  return print_test_name_impl