import pytest

import test_tools as tt

from .shared_utilites import prepared_proposal_data_with_id, funded_account_info, prepare_proposal


@pytest.fixture
def wallet(node) -> tt.Wallet:
    return tt.Wallet(attach_to=node)


@pytest.fixture
def creator(node) -> tt.Account:
    return tt.Account('initminer')


def create_funded_account(creator: tt.Account, wallet: tt.Wallet, id: int = 0) -> funded_account_info:
    result = funded_account_info()

    result.creator = creator

    result.account = tt.Account(f'actor-{id}')
    result.funded_TESTS = tt.Asset.Test(20)
    result.funded_TBD = tt.Asset.Tbd(20)
    result.funded_VESTS = tt.Asset.Test(200)

    tt.logger.info('importing key to wallet')
    wallet.api.import_key(result.account.private_key)
    tt.logger.info(f"imported key: {result.account.private_key} for account: {result.account.name}")

    with wallet.in_single_transaction():
        tt.logger.info('creating actor with keys')
        wallet.api.create_account_with_keys(
            creator=creator.name,
            newname=result.account.name,
            json_meta='{}',
            owner=result.account.public_key,
            active=result.account.public_key,
            posting=result.account.public_key,
            memo=result.account.public_key
        )

        tt.logger.info('transferring TESTS')
        wallet.api.transfer(
            from_=creator.name,
            to=result.account.name,
            amount=result.funded_TESTS,
            memo='TESTS'
        )

        tt.logger.info('transferring TBD')
        wallet.api.transfer(
            from_=creator.name,
            to=result.account.name,
            amount=result.funded_TBD,
            memo='TBD'
        )

    tt.logger.info('transfer to VESTing')
    wallet.api.transfer_to_vesting(
        from_=creator.name,
        to=result.account.name,
        amount=result.funded_VESTS
    )

    return result


@pytest.fixture
def funded_account(creator: tt.Account, wallet: tt.Wallet, id: int = 0) -> funded_account_info:
    return create_funded_account(creator=creator, wallet=wallet, id=id)


def create_proposal(wallet: tt.Wallet, funded_account: funded_account_info, creator_is_propsal_creator: bool):
    prepared_proposal = prepare_proposal(funded_account, 'initial', author_is_creator=creator_is_propsal_creator)
    wallet.api.post_comment(**prepared_proposal.post_comment_arguments)
    wallet.api.create_proposal(**prepared_proposal.create_proposal_arguments)
    response = wallet.api.list_proposals(start=[''], limit=50, order_by='by_creator', order_type='ascending',
                                         status='all')
    for prop in response:
        if prop['permlink'] == prepared_proposal.permlink:
            return prepared_proposal_data_with_id(base=prepared_proposal, id=prop['id'])

    assert False, 'proposal not found'


@pytest.fixture
def creator_proposal_id(wallet: tt.Wallet, funded_account: funded_account_info) -> prepared_proposal_data_with_id:
    return create_proposal(wallet=wallet, funded_account=funded_account, creator_is_propsal_creator=True)


@pytest.fixture
def account_proposal_id(wallet: tt.Wallet, funded_account: funded_account_info) -> prepared_proposal_data_with_id:
    return create_proposal(wallet=wallet, funded_account=funded_account, creator_is_propsal_creator=False)
