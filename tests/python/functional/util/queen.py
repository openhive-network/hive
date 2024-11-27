import test_tools as tt


def test_try_queen():
    acs = tt.AlternateChainSpecs(
        genesis_time=int(tt.Time.now(serialize=False).timestamp()),
        hardfork_schedule=[tt.HardforkSchedule(hardfork=27, block_num=1)],
    )
    node = tt.InitNode()
    node.config.plugin.append("queen")
    # node.run(wait_for_live=False, alternate_chain_specs=acs)
    node.run(alternate_chain_specs=acs)
    print()
    wallet = tt.Wallet(attach_to=node)
    wallet.api.set_transaction_expiration(100)

    node.api.debug_node.debug_generate_blocks(
        debug_key=tt.Account("initminer").private_key,
        count=10,
        skip=0,
        miss_blocks=0,
        edit_if_needed=True,
    )

    wallet.api.transfer("initminer", "initminer", tt.Asset.Test(500), "memo")
    #
    # for i in range(10_000):
    #     wallet.api.create_account("initminer", f"account-{i}", "{}")
    # print()
    #
