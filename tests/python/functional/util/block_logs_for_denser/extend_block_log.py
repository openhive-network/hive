import datetime
from pathlib import Path

import pytz

import test_tools as tt
from hive_local_tools.constants import HIVE_GENESIS_TIME, MIRRORNET_5M_WITNESSES, MIRRORNET_SKELETON_KEY


def prepare_blocklog_for_denser_testing():
    block_log_directory = Path('/home/dev/block_log_for_testing_denser_chain_id_44')
    block_log = tt.BlockLog(block_log_directory / "block_log")

    node = tt.InitNode()
    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    node.config.shared_file_size = "2G"
    node.config.enable_stale_production = 1
    node.config.required_participation = 0
    node.config.plugin.append("database_api")
    node.config.plugin.append("wallet_bridge_api")
    node.config.private_key = MIRRORNET_SKELETON_KEY
    for witness in MIRRORNET_5M_WITNESSES:
        node.config.witness.append(witness)

    # replay block_log with 5m mirrornet blocks
    node.run(
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=5),
        timeout=2400,
        replay_from=block_log,
        arguments=[f"--chain-id=44"],
        exit_before_synchronization=True,
        alternate_chain_specs=tt.AlternateChainSpecs(
            hf_21_stall_block=0,
            genesis_time=HIVE_GENESIS_TIME,
            init_supply=410_000_000_000,
            hbd_init_supply=30_000_000_000,
            initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=170_000_000_000),
            hardfork_schedule=[
                tt.HardforkSchedule(hardfork=1, block_num=convert("2016-04-25 17:30:00")),
                tt.HardforkSchedule(hardfork=2, block_num=convert("2016-04-26 18:00:15")),
                tt.HardforkSchedule(hardfork=3, block_num=convert("2016-04-27 13:00:00")),
                tt.HardforkSchedule(hardfork=4, block_num=convert("2016-04-30 15:00:00")),
                tt.HardforkSchedule(hardfork=5, block_num=convert("2016-05-31 17:00:00")),
                tt.HardforkSchedule(hardfork=6, block_num=convert("2016-06-30 14:00:00")),
                tt.HardforkSchedule(hardfork=7, block_num=convert("2016-07-04 00:00:00")),
                tt.HardforkSchedule(hardfork=8, block_num=convert("2016-07-04 01:00:00")),
                tt.HardforkSchedule(hardfork=9, block_num=convert("2016-07-14 23:05:30")),
                tt.HardforkSchedule(hardfork=10, block_num=convert("2016-07-16 07:18:18")),
                tt.HardforkSchedule(hardfork=11, block_num=convert("2016-07-17 15:00:00")),
                tt.HardforkSchedule(hardfork=12, block_num=convert("2016-07-26 15:00:03")),
                tt.HardforkSchedule(hardfork=13, block_num=convert("2016-08-15 14:00:00")),
                tt.HardforkSchedule(hardfork=current_hardfork_number, block_num=block_log.get_head_block_number() + 10)]
        )
    )

    # run node and apply hardforks to current version
    node.run(
        timeout=120,
        arguments=[f"--chain-id=44"],
        time_control=tt.StartTimeControl(start_time="head_block_time", speed_up_rate=5),
        alternate_chain_specs=tt.AlternateChainSpecs(
            hf_21_stall_block=0,
            genesis_time=HIVE_GENESIS_TIME,
            init_supply=410_000_000_000,
            hbd_init_supply=30_000_000_000,
            initial_vesting=tt.InitialVesting(vests_per_hive=1800, hive_amount=170_000_000_000),
            hardfork_schedule=[
                tt.HardforkSchedule(hardfork=1, block_num=convert("2016-04-25 17:30:00")),
                tt.HardforkSchedule(hardfork=2, block_num=convert("2016-04-26 18:00:15")),
                tt.HardforkSchedule(hardfork=3, block_num=convert("2016-04-27 13:00:00")),
                tt.HardforkSchedule(hardfork=4, block_num=convert("2016-04-30 15:00:00")),
                tt.HardforkSchedule(hardfork=5, block_num=convert("2016-05-31 17:00:00")),
                tt.HardforkSchedule(hardfork=6, block_num=convert("2016-06-30 14:00:00")),
                tt.HardforkSchedule(hardfork=7, block_num=convert("2016-07-04 00:00:00")),
                tt.HardforkSchedule(hardfork=8, block_num=convert("2016-07-04 01:00:00")),
                tt.HardforkSchedule(hardfork=9, block_num=convert("2016-07-14 23:05:30")),
                tt.HardforkSchedule(hardfork=10, block_num=convert("2016-07-16 07:18:18")),
                tt.HardforkSchedule(hardfork=11, block_num=convert("2016-07-17 15:00:00")),
                tt.HardforkSchedule(hardfork=12, block_num=convert("2016-07-26 15:00:03")),
                tt.HardforkSchedule(hardfork=13, block_num=convert("2016-08-15 14:00:00")),
                tt.HardforkSchedule(hardfork=current_hardfork_number, block_num=block_log.get_head_block_number() + 10)]
        )
    )

    wait_for_current_hardfork(node, current_hardfork_number)


    ### tutaj dodać co tam chcą denserowcy
    wallet = tt.Wallet(attach_to=node)
    ###

    node.wait_for_irreversible_block()

    timestamp = node.api.block.get_block(block_num=node.get_last_block_number())["block"]["timestamp"]
    tt.logger.info(f"Final block_log head block number: {node.get_last_block_number()}")
    tt.logger.info(f"Final block_log head block timestamp: {timestamp}")

    node.close()
    node.block_log.copy_to(Path(__file__).parent.absolute())


def wait_for_current_hardfork(node, current_hardfork_number) -> None:
    def is_current_hardfork() -> bool:
        version = node.api.wallet_bridge.get_hardfork_version()
        return int(version.split(".")[1]) >= current_hardfork_number

    tt.logger.info("Wait for current hardfork...")
    tt.Time.wait_for(is_current_hardfork)
    tt.logger.info(f"Current Hardfork {current_hardfork_number} applied.")


def convert(date_string: str) -> int:
    date_object = datetime.datetime.strptime(date_string, "%Y-%m-%d %H:%M:%S")
    gmt_timezone = pytz.timezone('GMT')
    gmt_datetime = date_object.replace(tzinfo=pytz.utc).astimezone(gmt_timezone)
    block = int(gmt_datetime.timestamp()) - HIVE_GENESIS_TIME
    return int(block / 3)


if __name__ == '__main__':
    prepare_blocklog_for_denser_testing()
