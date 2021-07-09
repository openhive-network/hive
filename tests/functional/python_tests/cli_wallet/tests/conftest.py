import pytest
from test_tools import *
from test_tools.private.node import Node
from test_tools.wallet import Wallet
from .shared_utilites import funded_account_info

@pytest.fixture
def node(world : World):
	node = world.create_init_node()
	node.run()
	return node

@pytest.fixture
def wallet(node : Node):
	return node.attach_wallet()

@pytest.fixture
def funded_account(node : Node, wallet : Wallet) -> funded_account_info:
	result = funded_account_info()

	result.creator = Account('initminer', with_keys=False)
	result.creator.private_key = node.config.private_key

	result.account = Account('actor')
	result.funded_TESTS = Asset.Test(20)
	result.funded_TBD = Asset.Tbd(20)
	result.funded_VESTS = Asset.Test(200)

	logger.info('importing key to wallet')
	wallet.api.import_key(result.account.private_key)

	logger.info('creating actor with keys')
	wallet.api.create_account_with_keys(
		creator=result.creator.name,
		newname=result.account.name,
		json_meta='{}',
		owner=result.account.public_key,
		active=result.account.public_key,
		posting=result.account.public_key,
		memo=result.account.public_key
	)

	logger.info('transferring TESTS')
	wallet.api.transfer(
		from_=result.creator.name,
		to=result.account.name,
		amount=result.funded_TESTS,
		memo='TESTS'
	)

	logger.info('transferring TBD')
	wallet.api.transfer(
		from_=result.creator.name,
		to=result.account.name,
		amount=result.funded_TBD,
		memo='TBD'
	)

	logger.info('transfer to VESTing')
	wallet.api.transfer_to_vesting(
		from_=result.creator.name,
		to=result.account.name,
		amount=result.funded_VESTS
	)

	return result