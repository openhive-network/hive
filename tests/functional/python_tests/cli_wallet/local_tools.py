import test_tools as tt

from .shared_utilites import funded_account_info


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
