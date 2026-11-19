from __future__ import annotations

import json
from typing import Final

import test_tools as tt

# far enough to send transactions on HF28 first, the node runs faster than real time
HF29_BLOCK: Final[int] = 60
# well above min_delegation of default testnet (around 3600 RC), see test_list_direct_rc_delegations.py
DELEGATION: Final[int] = 100_000
SERIALIZATIONS: Final[tuple[str, ...]] = ("legacy", "hf26")


def test_delegate_rc_in_cli_wallet() -> None:
    """
    Before HF29 cli_wallet embeds delegate_rc_operation in custom_json_operation with id "rc",
    since HF29 it puts delegate_rc_operation directly in the transaction (it asks the node which hardfork is active).
    """
    # hardfork scheduled after genesis needs votes of 17 witnesses (HIVE_HARDFORK_REQUIRED_WITNESSES)
    witnesses = [f"witness-{num}" for num in range(20)]
    node = tt.InitNode()
    node.config.witness.extend(witnesses)
    node.run(
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5),
        alternate_chain_specs=tt.AlternateChainSpecs(
            genesis_time=int(tt.Time.now(serialize=False).timestamp()),
            hardfork_schedule=[
                tt.HardforkSchedule(hardfork=28, block_num=1),
                tt.HardforkSchedule(hardfork=29, block_num=HF29_BLOCK),
            ],
            init_witnesses=witnesses,
        ),
    )
    wallets = {
        serialization: tt.OldWallet(
            attach_to=node, additional_arguments=[f"--transaction-serialization={serialization}"]
        )
        for serialization in SERIALIZATIONS
    }
    # one delegatee per wallet, so every delegation can be checked separately
    delegatees = {"legacy": "alice", "hf26": "bob"}
    for delegatee in delegatees.values():
        wallets["hf26"].create_account(delegatee)

    assert get_hardfork_version(node) == "1.28.0"
    for serialization, wallet in wallets.items():
        transaction = wallet.api.delegate_rc("initminer", [delegatees[serialization]], DELEGATION)
        tt.logger.info(f"delegate_rc from {serialization} wallet before HF29 in block {transaction['block_num']}")
        assert_embedded_delegate_rc(transaction, serialization, delegatee=delegatees[serialization], max_rc=DELEGATION)
    assert get_hardfork_version(node) == "1.28.0", "HF29 activated too early, increase HF29_BLOCK"
    assert get_delegations(wallets["hf26"]) == {"alice": DELEGATION, "bob": DELEGATION}

    node.wait_for_block_with_number(HF29_BLOCK)
    # votes are counted once per schedule round (21 blocks), a missed block can delay them
    for _ in range(2 * 21):
        if get_hardfork_version(node) == "1.29.0":
            break
        node.wait_number_of_blocks(1)
    assert get_hardfork_version(node) == "1.29.0"

    for serialization, wallet in wallets.items():
        transaction = wallet.api.delegate_rc("initminer", [delegatees[serialization]], 2 * DELEGATION)
        assert_direct_delegate_rc(
            transaction, serialization, delegatee=delegatees[serialization], max_rc=2 * DELEGATION
        )
    assert get_delegations(wallets["hf26"]) == {"alice": 2 * DELEGATION, "bob": 2 * DELEGATION}


def get_hardfork_version(node: tt.InitNode) -> str:
    return node.api.database.get_hardfork_properties().current_hardfork_version


def get_delegations(wallet: tt.OldWallet) -> dict[str, int]:
    return {
        delegation["to"]: delegation["delegated_rc"]
        for delegation in wallet.api.list_rc_direct_delegations(["initminer", ""], 100)
    }


def get_single_operation(transaction: dict, serialization: str) -> tuple[str, dict]:
    assert len(transaction["operations"]) == 1
    operation = transaction["operations"][0]
    if serialization == "legacy":
        return operation[0], operation[1]
    return operation["type"], operation["value"]


def expected_delegate_rc(delegatee: str, max_rc: int) -> dict:
    return {"from": "initminer", "delegatees": [delegatee], "max_rc": max_rc, "extensions": []}


def assert_embedded_delegate_rc(transaction: dict, serialization: str, *, delegatee: str, max_rc: int) -> None:
    name, value = get_single_operation(transaction, serialization)
    assert name == ("custom_json" if serialization == "legacy" else "custom_json_operation")
    assert value["id"] == "rc"
    assert value["required_auths"] == []
    assert value["required_posting_auths"] == ["initminer"]
    assert json.loads(value["json"]) == {
        "type": "delegate_rc_operation",
        "value": expected_delegate_rc(delegatee, max_rc),
    }


def assert_direct_delegate_rc(transaction: dict, serialization: str, *, delegatee: str, max_rc: int) -> None:
    name, value = get_single_operation(transaction, serialization)
    assert name == ("delegate_rc" if serialization == "legacy" else "delegate_rc_operation")
    assert value == expected_delegate_rc(delegatee, max_rc)
