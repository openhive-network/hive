import pytest
from test_tools import *
from test_tools.private.node import Node
from test_tools import Wallet
from .shared_utilites import prepared_proposal_data_with_id, funded_account_info, prepare_proposal
import datetime, dateutil

@pytest.fixture(scope='function')
def node(world : World):
  node = world.create_init_node()
  node.run()
  return node

@pytest.fixture
def wallet(node : Node):
  return Wallet(attach_to=node)

@pytest.fixture
def creator(node : Node) -> Account:
  result = Account('initminer', with_keys=False)
  result.private_key = node.config.private_key
  return result

def create_funded_account(creator : Account, wallet : Wallet, id : int = 0) -> funded_account_info:
  result = funded_account_info()

  result.creator = creator

  result.account = Account(f'actor-{id}')
  result.funded_TESTS = Asset.Test(20)
  result.funded_TBD = Asset.Tbd(20)
  result.funded_VESTS = Asset.Test(200)

  logger.info('importing key to wallet')
  wallet.api.import_key(result.account.private_key)
  logger.info(f"imported key: {result.account.private_key} for account: {result.account.name}")

  with wallet.in_single_transaction():
    logger.info('creating actor with keys')
    wallet.api.create_account_with_keys(
      creator=creator.name,
      newname=result.account.name,
      json_meta='{}',
      owner=result.account.public_key,
      active=result.account.public_key,
      posting=result.account.public_key,
      memo=result.account.public_key
    )

    logger.info('transferring TESTS')
    wallet.api.transfer(
      from_=creator.name,
      to=result.account.name,
      amount=result.funded_TESTS,
      memo='TESTS'
    )

    logger.info('transferring TBD')
    wallet.api.transfer(
      from_=creator.name,
      to=result.account.name,
      amount=result.funded_TBD,
      memo='TBD'
    )

  logger.info('transfer to VESTing')
  wallet.api.transfer_to_vesting(
    from_=creator.name,
    to=result.account.name,
    amount=result.funded_VESTS
  )

  return result

@pytest.fixture
def funded_account(creator : Account, wallet : Wallet, id : int = 0) -> funded_account_info:
  return create_funded_account(creator=creator, wallet=wallet, id=id)

def create_proposal(wallet : Wallet, funded_account : funded_account_info, creator_is_propsal_creator : bool):
  prepared_proposal = prepare_proposal(funded_account, 'initial', author_is_creator=creator_is_propsal_creator)
  wallet.api.post_comment(**prepared_proposal.post_comment_arguments)
  wallet.api.create_proposal(**prepared_proposal.create_proposal_arguments)
  response = wallet.api.list_proposals(start=[''], limit=50, order_by='by_creator', order_type='ascending', status='all')['result']
  for prop in response:
    if prop['permlink'] == prepared_proposal.permlink:
      return prepared_proposal_data_with_id(base = prepared_proposal, id =prop['id'])
  assert False, 'proposal not found'


@pytest.fixture
def creator_proposal_id(wallet : Wallet, funded_account : funded_account_info) -> prepared_proposal_data_with_id:
  return create_proposal(wallet=wallet, funded_account=funded_account, creator_is_propsal_creator=True)

@pytest.fixture
def account_proposal_id(wallet : Wallet, funded_account : funded_account_info) -> prepared_proposal_data_with_id:
  return create_proposal(wallet=wallet, funded_account=funded_account, creator_is_propsal_creator=False)

