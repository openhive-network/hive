from __future__ import annotations

import pytest

import test_tools as tt


@pytest.mark.parametrize(
    ("hbd_amount", "hive_amount"),
    [
        (tt.Asset.Tbd(50), tt.Asset.Test(0)),
        (tt.Asset.Tbd(0), tt.Asset.Test(50)),
        (tt.Asset.Tbd(25), tt.Asset.Test(25)),
    ],
    ids=("only_hbd", "only_hive", "hbd_and_hive"),
)
@pytest.mark.testnet()
def test_create_escrow_transfer(prepare_escrow, hbd_amount, hive_amount) -> None:
    # Test case 1.1 from https://gitlab.syncad.com/hive/hive/-/issues/638
    escrow = prepare_escrow
    escrow.assert_if_escrow_was_created()
    escrow.from_account.check_if_current_rc_mana_was_reduced(escrow.trx)
    escrow.from_account.check_account_balance(escrow.trx, mode="escrow_creation")


@pytest.mark.parametrize(
    ("hbd_amount", "hive_amount"),
    [
        (tt.Asset.Tbd(50), tt.Asset.Test(0)),
        (tt.Asset.Tbd(0), tt.Asset.Test(50)),
        (tt.Asset.Tbd(25), tt.Asset.Test(25)),
    ],
    ids=("only_hbd", "only_hive", "hbd_and_hive"),
)
@pytest.mark.testnet()
def test_approve_escrow_transfer(prepare_escrow, hbd_amount, hive_amount) -> None:
    # Test case 2.1 from https://gitlab.syncad.com/hive/hive/-/issues/638
    escrow = prepare_escrow
    escrow.assert_if_escrow_was_created()
    escrow.from_account.check_if_current_rc_mana_was_reduced(escrow.trx)
    escrow.from_account.check_account_balance(escrow.trx, mode="escrow_creation")

    for approval_author in (escrow.agent, escrow.to_account):
        approve_trx = escrow.approve(approval_author, approve=True)
        approval_author.check_if_current_rc_mana_was_reduced(approve_trx)

    escrow.assert_escrow_approved_operation_was_generated(expected_amount=1)
    escrow.agent.check_if_escrow_fee_was_added_to_agent_balance_after_approvals(escrow.trx)


@pytest.mark.parametrize(
    ("hbd_amount", "hive_amount"),
    [
        (tt.Asset.Tbd(50), tt.Asset.Test(0)),
        (tt.Asset.Tbd(0), tt.Asset.Test(50)),
        (tt.Asset.Tbd(25), tt.Asset.Test(25)),
    ],
    ids=("only_hbd", "only_hive", "hbd_and_hive"),
)
@pytest.mark.parametrize(
    ("approval_author", "rejection_author", "time_jump"),
    [
        ("agent", "no one", True),
        ("to_account", "no one", True),
        ("no one", "no one", True),
        ("no one", "agent", False),
        ("no one", "to_account", False),
        ("agent", "to_account", False),
        ("to_account", "agent", False),
    ],
)
@pytest.mark.testnet()
def test_escrow_rejection(
    prepared_node, prepare_escrow, hbd_amount, hive_amount, approval_author, rejection_author, time_jump
) -> None:
    # Test cases 2.2 - 2.8 from https://gitlab.syncad.com/hive/hive/-/issues/638
    escrow = prepare_escrow
    escrow.assert_if_escrow_was_created()
    escrow.from_account.check_if_current_rc_mana_was_reduced(escrow.trx)
    escrow.from_account.check_account_balance(escrow.trx, mode="escrow_creation")

    if approval_author != "no one":
        approval_author = getattr(escrow, approval_author)
        approve_trx = escrow.approve(approval_author, approve=True)
        approval_author.check_if_current_rc_mana_was_reduced(approve_trx)

    if rejection_author != "no one":
        rejection_author = getattr(escrow, rejection_author)
        reject_trx = escrow.approve(rejection_author, approve=False)
        rejection_author.check_if_current_rc_mana_was_reduced(reject_trx)

    # optionally jump after ratification deadline
    if time_jump:
        start_time = prepared_node.get_head_block_time() + tt.Time.weeks(2)
        prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))
    escrow.assert_escrow_rejected_operation_was_generated(expected_amount=1)
    escrow.from_account.check_account_balance(escrow.trx, mode="escrow_rejection")


@pytest.mark.parametrize(
    ("release_author", "release_target", "hbd_amount", "hive_amount", "time_jump"),
    [
        # Test case 3.1 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("agent", "from_account", tt.Asset.Tbd(0), tt.Asset.Test(50), False),
        ("agent", "from_account", tt.Asset.Tbd(50), tt.Asset.Test(0), False),
        # Test case 4.1 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("agent", "from_account", tt.Asset.Tbd(0), tt.Asset.Test(50), True),
        ("agent", "from_account", tt.Asset.Tbd(50), tt.Asset.Test(0), True),
        # Test case 3.2 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("agent", "to_account", tt.Asset.Tbd(0), tt.Asset.Test(50), False),
        ("agent", "to_account", tt.Asset.Tbd(50), tt.Asset.Test(0), False),
        # Test case 4.2 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("agent", "to_account", tt.Asset.Tbd(50), tt.Asset.Test(0), True),
        ("agent", "to_account", tt.Asset.Tbd(0), tt.Asset.Test(50), True),
        # Test case 3.3 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("from_account", "from_account", tt.Asset.Tbd(0), tt.Asset.Test(50), False),
        ("from_account", "from_account", tt.Asset.Tbd(50), tt.Asset.Test(0), False),
        # Test case 3.4 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("to_account", "to_account", tt.Asset.Tbd(0), tt.Asset.Test(50), False),
        ("to_account", "to_account", tt.Asset.Tbd(50), tt.Asset.Test(0), False),
    ],
)
@pytest.mark.testnet()
def test_try_to_release_escrow_with_forbidden_arguments_before_or_after_expiration(
    prepared_node, prepare_escrow, hbd_amount, hive_amount, release_author, release_target, time_jump
) -> None:
    escrow = prepare_escrow
    escrow.assert_if_escrow_was_created()
    escrow.from_account.check_if_current_rc_mana_was_reduced(escrow.trx)
    escrow.from_account.check_account_balance(escrow.trx, mode="escrow_creation")

    for approval_author in (escrow.agent, escrow.to_account):
        approve_trx = escrow.approve(approval_author, approve=True)
        approval_author.check_if_current_rc_mana_was_reduced(approve_trx)

    escrow.agent.check_if_escrow_fee_was_added_to_agent_balance_after_approvals(escrow.trx)
    release_author = getattr(escrow, release_author)
    release_target = getattr(escrow, release_target)

    # optionally jump after expiration time
    if time_jump:
        start_time = prepared_node.get_head_block_time() + tt.Time.weeks(4)
        prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))

    with pytest.raises(tt.exceptions.CommunicationError) as exception:
        escrow.release(who=release_author, receiver=release_target, hbd_amount=hbd_amount, hive_amount=hive_amount)
    assert "release" in exception.value.error, "Message other than expected."


@pytest.mark.parametrize(
    (
        "release_author",
        "release_target",
        "hbd_amount",
        "hive_amount",
        "escrow_hbd_release_amount",
        "escrow_hive_release_amount",
        "escrow_releases",
        "time_jump",
    ),
    [
        # BEFORE EXPIRATION
        # Test case 3.5 from https://gitlab.syncad.com/hive/hive/-/issues/638
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            1,
            False,
        ),
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            1,
            False,
        ),
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(25),
            tt.Asset.Test(0),
            1,
            False,
        ),
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(25),
            1,
            False,
        ),
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            1,
            False,
        ),
        # Test case 3.6 from https://gitlab.syncad.com/hive/hive/-/issues/638
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            1,
            False,
        ),
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            1,
            False,
        ),
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(25),
            tt.Asset.Test(0),
            1,
            False,
        ),
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(25),
            1,
            False,
        ),
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            1,
            False,
        ),
        # Test case 3.7 from https://gitlab.syncad.com/hive/hive/-/issues/638
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(25),
            2,
            False,
        ),
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(25),
            tt.Asset.Test(0),
            2,
            False,
        ),
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(50),
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            2,
            False,
        ),
        # Test case 3.8 from https://gitlab.syncad.com/hive/hive/-/issues/638
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(25),
            2,
            False,
        ),
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(25),
            tt.Asset.Test(0),
            2,
            False,
        ),
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(50),
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            2,
            False,
        ),
        # AFTER EXPIRATION
        # Test case 4.3 from https://gitlab.syncad.com/hive/hive/-/issues/638
        (
            "from_account",
            "from_account",
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            1,
            True,
        ),
        (
            "from_account",
            "from_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            1,
            True,
        ),
        (
            "from_account",
            "from_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(25),
            tt.Asset.Test(0),
            1,
            True,
        ),
        (
            "from_account",
            "from_account",
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(25),
            1,
            True,
        ),
        (
            "from_account",
            "from_account",
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            1,
            True,
        ),
        # Test case 4.4 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("to_account", "to_account", tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(50), 1, True),
        ("to_account", "to_account", tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(50), tt.Asset.Test(0), 1, True),
        ("to_account", "to_account", tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(25), tt.Asset.Test(0), 1, True),
        ("to_account", "to_account", tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(25), 1, True),
        ("to_account", "to_account", tt.Asset.Tbd(25), tt.Asset.Test(25), tt.Asset.Tbd(25), tt.Asset.Test(25), 1, True),
        #
        # Test case 4.5 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("from_account", "to_account", tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(50), tt.Asset.Test(0), 1, True),
        ("from_account", "to_account", tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(50), 1, True),
        ("from_account", "to_account", tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(25), tt.Asset.Test(0), 1, True),
        ("from_account", "to_account", tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(25), 1, True),
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            1,
            True,
        ),
        # Test case 4.6 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("to_account", "from_account", tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(50), tt.Asset.Test(0), 1, True),
        ("to_account", "from_account", tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(50), 1, True),
        ("to_account", "from_account", tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(25), tt.Asset.Test(0), 1, True),
        ("to_account", "from_account", tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(25), 1, True),
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            1,
            True,
        ),
        # Test case 4.7 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("from_account", "to_account", tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(25), 2, True),
        ("from_account", "to_account", tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(25), tt.Asset.Test(0), 2, True),
        (
            "from_account",
            "to_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(50),
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            2,
            True,
        ),
        # Test case 4.8 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("to_account", "from_account", tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(25), 2, True),
        ("to_account", "from_account", tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(25), tt.Asset.Test(0), 2, True),
        (
            "to_account",
            "from_account",
            tt.Asset.Tbd(50),
            tt.Asset.Test(50),
            tt.Asset.Tbd(25),
            tt.Asset.Test(25),
            2,
            True,
        ),
    ],
)
@pytest.mark.testnet()
def test_release_escrow_before_or_after_expiration(
    prepared_node,
    prepare_escrow,
    release_author,
    release_target,
    hbd_amount,
    hive_amount,
    escrow_hbd_release_amount,
    escrow_hive_release_amount,
    escrow_releases,
    time_jump,
) -> None:
    escrow = prepare_escrow
    escrow.assert_if_escrow_was_created()
    escrow.from_account.check_if_current_rc_mana_was_reduced(escrow.trx)
    escrow.from_account.check_account_balance(escrow.trx, mode="escrow_creation")

    for approval_author in (escrow.agent, escrow.to_account):
        approve_trx = escrow.approve(approval_author, approve=True)
        approval_author.check_if_current_rc_mana_was_reduced(approve_trx)

    escrow.agent.check_if_escrow_fee_was_added_to_agent_balance_after_approvals(escrow.trx)
    # optionally jump after expiration time
    if time_jump:
        start_time = prepared_node.get_head_block_time() + tt.Time.weeks(4)
        prepared_node.restart(time_control=tt.StartTimeControl(start_time=start_time))
    release_author = getattr(escrow, release_author)
    release_target = getattr(escrow, release_target)
    for _release in range(escrow_releases):
        release_trx = escrow.release(
            who=release_author,
            receiver=release_target,
            hbd_amount=escrow_hbd_release_amount,
            hive_amount=escrow_hive_release_amount,
        )
        release_author.check_if_current_rc_mana_was_reduced(release_trx)
        release_target.check_account_balance(release_trx, mode="escrow_release")


@pytest.mark.parametrize(
    ("hbd_amount", "hive_amount"),
    [
        (tt.Asset.Tbd(0), tt.Asset.Test(50)),
        (tt.Asset.Tbd(50), tt.Asset.Test(0)),
    ],
)
@pytest.mark.parametrize(
    "dispute_author",
    ["from_account", "to_account"],
)
@pytest.mark.testnet()
def test_try_to_release_escrow_with_dispute(prepare_escrow, hbd_amount, hive_amount, dispute_author) -> None:
    # test cases 5.1, 5.2 from https://gitlab.syncad.com/hive/hive/-/issues/638

    escrow = prepare_escrow
    escrow.assert_if_escrow_was_created()
    escrow.from_account.check_if_current_rc_mana_was_reduced(escrow.trx)
    escrow.from_account.check_account_balance(escrow.trx, mode="escrow_creation")

    for approval_author in (escrow.agent, escrow.to_account):
        approve_trx = escrow.approve(approval_author, approve=True)
        approval_author.check_if_current_rc_mana_was_reduced(approve_trx)

    dispute_author = getattr(escrow, dispute_author)
    dispute_trx = escrow.dispute(who=dispute_author)
    dispute_author.check_if_current_rc_mana_was_reduced(dispute_trx)
    escrow.assert_if_escrow_is_disputed()
    for release_author, release_target in zip(
        (escrow.to_account, escrow.from_account), (escrow.from_account, escrow.to_account)
    ):
        with pytest.raises(tt.exceptions.CommunicationError) as exception:
            escrow.release(who=release_author, receiver=release_target, hbd_amount=hbd_amount, hive_amount=hive_amount)
        assert "release funds in a disputed escrow" in exception.value.error, "Message other than expected."


@pytest.mark.parametrize(
    (
        "dispute_author",
        "release_targets",
        "hbd_amount",
        "hive_amount",
        "escrow_hbd_release_amount",
        "escrow_hive_release_amount",
    ),
    [
        # test case 5.3 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("from_account", ["from_account"], tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(50), tt.Asset.Test(0)),
        ("from_account", ["from_account"], tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(50)),
        # test case 5.4 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("from_account", ["to_account"], tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(50), tt.Asset.Test(0)),
        ("from_account", ["to_account"], tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(50)),
        # test case 5.5 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("to_account", ["from_account"], tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(50), tt.Asset.Test(0)),
        ("to_account", ["from_account"], tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(50)),
        # test case 5.6 from https://gitlab.syncad.com/hive/hive/-/issues/638
        ("to_account", ["to_account"], tt.Asset.Tbd(50), tt.Asset.Test(0), tt.Asset.Tbd(50), tt.Asset.Test(0)),
        ("to_account", ["to_account"], tt.Asset.Tbd(0), tt.Asset.Test(50), tt.Asset.Tbd(0), tt.Asset.Test(50)),
        # test case 5.7 from https://gitlab.syncad.com/hive/hive/-/issues/638
        (
            "from_account",
            ["to_account", "from_account"],
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(25),
            tt.Asset.Test(0),
        ),
        (
            "from_account",
            ["to_account", "from_account"],
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(25),
        ),
        # test case 5.8 from https://gitlab.syncad.com/hive/hive/-/issues/638
        (
            "to_account",
            ["to_account", "from_account"],
            tt.Asset.Tbd(50),
            tt.Asset.Test(0),
            tt.Asset.Tbd(25),
            tt.Asset.Test(0),
        ),
        (
            "to_account",
            ["to_account", "from_account"],
            tt.Asset.Tbd(0),
            tt.Asset.Test(50),
            tt.Asset.Tbd(0),
            tt.Asset.Test(25),
        ),
    ],
)
@pytest.mark.testnet()
def test_agent_releases_escrow_with_dispute(
    prepare_escrow,
    dispute_author,
    release_targets,
    hbd_amount,
    hive_amount,
    escrow_hbd_release_amount,
    escrow_hive_release_amount,
) -> None:
    escrow = prepare_escrow
    escrow.assert_if_escrow_was_created()
    escrow.from_account.check_if_current_rc_mana_was_reduced(escrow.trx)
    escrow.from_account.check_account_balance(escrow.trx, mode="escrow_creation")

    for approval_author in (escrow.agent, escrow.to_account):
        approve_trx = escrow.approve(approval_author, approve=True)
        approval_author.check_if_current_rc_mana_was_reduced(approve_trx)

    dispute_author = getattr(escrow, dispute_author)
    dispute_trx = escrow.dispute(who=dispute_author)
    dispute_author.check_if_current_rc_mana_was_reduced(dispute_trx)
    escrow.assert_if_escrow_is_disputed()

    for release_target in release_targets:
        release_target = getattr(escrow, release_target)
        release_trx = escrow.release(
            who=escrow.agent,
            receiver=release_target,
            hbd_amount=escrow_hbd_release_amount,
            hive_amount=escrow_hive_release_amount,
        )
        escrow.agent.check_if_current_rc_mana_was_reduced(release_trx)
        release_target.check_account_balance(release_trx, mode="escrow_release")
