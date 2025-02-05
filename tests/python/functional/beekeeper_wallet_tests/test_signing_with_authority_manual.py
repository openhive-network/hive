from __future__ import annotations

import pytest
from helpy.exceptions import ErrorInResponseError

import test_tools as tt


def test_signing_with_authority(node: tt.InitNode) -> None:
    wallet = tt.Wallet(attach_to=node)
    wallet1 = tt.Wallet(attach_to=node)
    wallet2 = tt.Wallet(attach_to=node)

    alice = tt.Account("tst-alice")
    auth1 = tt.Account("tst-auth1")
    auth2 = tt.Account("tst-auth2")

    for account in [alice, auth1, auth2]:
        wallet.api.create_account_with_keys(
            "initminer",
            account.name,
            "",
            account.public_key,
            account.public_key,
            account.public_key,
            account.public_key,
        )
        wallet.api.transfer("initminer", account.name, tt.Asset.from_legacy("1.000 TESTS"), "test transfer")
        wallet.api.transfer_to_vesting("initminer", account.name, tt.Asset.from_legacy("1.000 TESTS"))

    wallet.api.import_key(alice.private_key)
    wallet1.api.import_key(auth1.private_key)
    wallet2.api.import_key(auth2.private_key)

    # TRIGGER and VERIFY
    wallet.api.transfer(alice.name, "initminer", tt.Asset.from_legacy("0.001 TESTS"), "this will work")

    wallet.api.update_account_auth_account(alice.name, "active", auth1.name, 1)
    wallet1.api.update_account_auth_account(auth1.name, "active", auth2.name, 1)
    # create circular authority dependency and test wallet behaves correctly, i.e. no duplicate signatures
    wallet1.api.update_account_auth_account(auth1.name, "active", alice.name, 1)

    tt.logger.info("sign with own account keys")
    wallet.api.transfer(
        alice.name, "initminer", tt.Asset.from_legacy("0.001 TESTS"), "this will work and does not print warnings"
    )

    # wallet1 and wallet2 can sign only with account authority
    tt.logger.info("sign with account authority, manual selection")
    wallet1.api.use_authority("active", "tst-auth1")
    wallet1.api.transfer(alice.name, "initminer", tt.Asset.from_legacy("0.001 TESTS"), "this will work")

    tt.logger.info("sign with authority of account authority, automatic selection")
    wallet2.api.use_automatic_authority()
    wallet2.api.transfer(
        alice.name,
        "initminer",
        tt.Asset.from_legacy("0.001 TESTS"),
        "this will choose authorities automatically and work",
    )
    tt.logger.info("successfully signed and broadcasted transactions with account authority")

    tt.logger.info("try signing with not available keys")
    wallet2.api.use_authority("active", "tst-alice")

    # negative test
    with pytest.raises(
        ErrorInResponseError, match=r"Public key STM[\w]{50} not found in InitNode\d+\.Wallet\d+ wallet"
    ):
        wallet2.api.transfer(alice.name, "initminer", tt.Asset.from_legacy("0.001 TESTS"), "this will NOT work")
