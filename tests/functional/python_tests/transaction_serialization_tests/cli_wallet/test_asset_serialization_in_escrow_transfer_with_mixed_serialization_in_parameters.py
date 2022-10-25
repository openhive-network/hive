import pytest

import test_tools as tt

from .....local_tools import date_from_now, run_for


@run_for('testnet')
def test_hbd_amount_in_legacy_serialization_with_hf26_wallet(wallet_with_hf26_serialization):
    with wallet_with_hf26_serialization.in_single_transaction():
        wallet_with_hf26_serialization.api.create_account('initminer', 'alice', '{}')
        wallet_with_hf26_serialization.api.create_account('initminer', 'bob', '{}')

    wallet_with_hf26_serialization.api.escrow_transfer('initminer', 'alice', 'bob', 99, tt.Asset.Tbd(1),
                                                       tt.Asset.Test(2).as_nai(), tt.Asset.Tbd(1).as_nai(),
                                                       date_from_now(weeks=16), date_from_now(weeks=20), '{}')


@run_for('testnet')
def test_hbd_amount_in_hf26_serialization_with_legacy_wallet(wallet_with_legacy_serialization):
    with wallet_with_legacy_serialization.in_single_transaction():
        wallet_with_legacy_serialization.api.create_account('initminer', 'alice', '{}')
        wallet_with_legacy_serialization.api.create_account('initminer', 'bob', '{}')

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet_with_legacy_serialization.api.escrow_transfer('initminer', 'alice', 'bob', 99, tt.Asset.Tbd(1).as_nai(),
                                                             tt.Asset.Test(2), tt.Asset.Tbd(1), date_from_now(weeks=16),
                                                             date_from_now(weeks=20), '{}')


@run_for('testnet')
def test_hive_amount_in_legacy_serialization_with_hf26_wallet(wallet_with_hf26_serialization):
    with wallet_with_hf26_serialization.in_single_transaction():
        wallet_with_hf26_serialization.api.create_account('initminer', 'alice', '{}')
        wallet_with_hf26_serialization.api.create_account('initminer', 'bob', '{}')

    wallet_with_hf26_serialization.api.escrow_transfer('initminer', 'alice', 'bob', 99, tt.Asset.Tbd(1).as_nai(),
                                                       tt.Asset.Test(2), tt.Asset.Tbd(1).as_nai(),
                                                       date_from_now(weeks=16), date_from_now(weeks=20), '{}')


@run_for('testnet')
def test_hive_amount_in_hf26_serialization_with_legacy_wallet(wallet_with_legacy_serialization):
    with wallet_with_legacy_serialization.in_single_transaction():
        wallet_with_legacy_serialization.api.create_account('initminer', 'alice', '{}')
        wallet_with_legacy_serialization.api.create_account('initminer', 'bob', '{}')

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet_with_legacy_serialization.api.escrow_transfer('initminer', 'alice', 'bob', 99, tt.Asset.Tbd(1),
                                                             tt.Asset.Test(2).as_nai(), tt.Asset.Tbd(1),
                                                             date_from_now(weeks=16), date_from_now(weeks=20), '{}')


@run_for('testnet')
def test_fee_in_legacy_serialization_with_hf26_wallet(wallet_with_hf26_serialization):
    with wallet_with_hf26_serialization.in_single_transaction():
        wallet_with_hf26_serialization.api.create_account('initminer', 'alice', '{}')
        wallet_with_hf26_serialization.api.create_account('initminer', 'bob', '{}')

    wallet_with_hf26_serialization.api.escrow_transfer('initminer', 'alice', 'bob', 99, tt.Asset.Tbd(1).as_nai(),
                                                       tt.Asset.Test(2).as_nai(), tt.Asset.Tbd(1),
                                                       date_from_now(weeks=16), date_from_now(weeks=20), '{}')


@run_for('testnet')
def test_fee_in_hf26_serialization_with_legacy_wallet(wallet_with_legacy_serialization):
    with wallet_with_legacy_serialization.in_single_transaction():
        wallet_with_legacy_serialization.api.create_account('initminer', 'alice', '{}')
        wallet_with_legacy_serialization.api.create_account('initminer', 'bob', '{}')

    with pytest.raises(tt.exceptions.CommunicationError):
        wallet_with_legacy_serialization.api.escrow_transfer('initminer', 'alice', 'bob', 99, tt.Asset.Tbd(1),
                                                             tt.Asset.Test(2), tt.Asset.Tbd(1).as_nai(),
                                                             date_from_now(weeks=16), date_from_now(weeks=20), '{}')
