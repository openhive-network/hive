from __future__ import annotations

import argparse
import datetime
from pathlib import Path

import pytz

import test_tools as tt
from hive_local_tools.constants import (
    HIVE_GENESIS_TIME,
    MIRRORNET_5M_WITNESSES,
    MIRRORNET_SKELETON_KEY,
)
from hive_local_tools.functional import wait_for_current_hardfork

parser = argparse.ArgumentParser()
parser.add_argument("--input-block-log-directory", type=Path)
parser.add_argument("--output-block-log-directory", type=Path, default=Path(__file__).parent)
args = parser.parse_args()

ACCOUNT_DETAILS = [
    ("denserautotest0", {}),
    ("denserautotest1", {"transfer": [tt.Asset.Hive(1), tt.Asset.Hbd(1)], "vesting": tt.Asset.Hive(1)}),
    ("denserautotest2", {"transfer": [tt.Asset.Hive(10)]}),
    ("denserautotest3", {"transfer": [tt.Asset.Hive(1000), tt.Asset.Hbd(100)], "vesting": tt.Asset.Hive(1000)}),
    ("denserautotest4", {"transfer": [tt.Asset.Hive(100), tt.Asset.Hbd(100)], "vesting": tt.Asset.Hive(100)}),
]


def generate_blocklog_for_denser(input_block_log_directory: Path) -> None:
    tt.logger.info("Start with block log from:", input_block_log_directory)

    block_log = tt.BlockLog(input_block_log_directory, "auto")

    node = tt.InitNode()
    current_hardfork_number = int(node.get_version()["version"]["blockchain_version"].split(".")[1])

    node.config.block_log_split = 9999 if block_log.is_split else -1
    node.config.shared_file_size = "2G"
    node.config.enable_stale_production = 1
    node.config.required_participation = 0
    node.config.plugin.append("database_api")
    node.config.plugin.append("wallet_bridge_api")
    node.config.private_key = MIRRORNET_SKELETON_KEY
    for witness in MIRRORNET_5M_WITNESSES:
        node.config.witness.append(witness)

    chain_parameters = tt.AlternateChainSpecs(
        hf_21_stall_block=0,
        min_root_comment_interval=1,
        genesis_time=HIVE_GENESIS_TIME,
        hardfork_schedule=[
            tt.HardforkSchedule(hardfork=1, block_num=get_absolute_slot_at_time("2016-04-25 17:30:00")),
            tt.HardforkSchedule(hardfork=2, block_num=get_absolute_slot_at_time("2016-04-26 18:00:15")),
            tt.HardforkSchedule(hardfork=3, block_num=get_absolute_slot_at_time("2016-04-27 13:00:00")),
            tt.HardforkSchedule(hardfork=4, block_num=get_absolute_slot_at_time("2016-04-30 15:00:00")),
            tt.HardforkSchedule(hardfork=5, block_num=get_absolute_slot_at_time("2016-05-31 17:00:00")),
            tt.HardforkSchedule(hardfork=6, block_num=get_absolute_slot_at_time("2016-06-30 14:00:00")),
            tt.HardforkSchedule(hardfork=7, block_num=get_absolute_slot_at_time("2016-07-04 00:00:00")),
            tt.HardforkSchedule(hardfork=8, block_num=get_absolute_slot_at_time("2016-07-04 01:00:00")),
            tt.HardforkSchedule(hardfork=9, block_num=get_absolute_slot_at_time("2016-07-14 23:05:30")),
            tt.HardforkSchedule(hardfork=10, block_num=get_absolute_slot_at_time("2016-07-16 07:18:18")),
            tt.HardforkSchedule(hardfork=11, block_num=get_absolute_slot_at_time("2016-07-17 15:00:00")),
            tt.HardforkSchedule(hardfork=12, block_num=get_absolute_slot_at_time("2016-07-26 15:00:03")),
            tt.HardforkSchedule(hardfork=13, block_num=get_absolute_slot_at_time("2016-08-15 14:00:00")),
            tt.HardforkSchedule(hardfork=current_hardfork_number, block_num=block_log.get_head_block_number() + 10),
        ],
    )

    # replay block_log with 5m mirrornet blocks
    node.run(
        time_control=tt.SpeedUpRateTimeControl(speed_up_rate=15),
        timeout=2400,
        replay_from=block_log,
        arguments=["--chain-id=44"],
        exit_before_synchronization=True,
        alternate_chain_specs=chain_parameters,
    )

    # run node and apply hardforks to current version
    node.run(
        timeout=120,
        arguments=["--chain-id=44"],
        time_control=tt.StartTimeControl(start_time="head_block_time", speed_up_rate=15),
        alternate_chain_specs=chain_parameters,
    )

    wait_for_current_hardfork(node, current_hardfork_number)

    wallet = tt.Wallet(attach_to=node, additional_arguments=["--chain-id=44"])

    for account_name, _ in ACCOUNT_DETAILS:
        import_keys(wallet, account_name)
        wallet.api.create_account_with_keys(
            "blocktrades",
            account_name,
            "{}",
            tt.PublicKey(account_name=account_name, secret="owner"),
            tt.PublicKey(account_name=account_name, secret="active"),
            tt.PublicKey(account_name=account_name, secret="posting"),
            tt.PublicKey(account_name=account_name, secret="memo"),
        )

    for account, ops in ACCOUNT_DETAILS:
        if "vesting" in ops:
            wallet.api.transfer_to_vesting("blocktrades", account, ops["vesting"])
        if "transfer" in ops:
            for asset in ops["transfer"]:
                wallet.api.transfer("blocktrades", account, asset, f"transfer-{asset.pretty_amount()}")

    for post_num in range(40):
        wallet.api.post_comment(
            "denserautotest4",
            f"post-test-{post_num}",
            "",
            "test",
            f"test-{post_num}",
            f"content Test {post_num} numer",
            "{}",
        )
        node.wait_number_of_blocks(1)
        tt.logger.info(f"Created post number: {post_num}.")

    node.wait_for_irreversible_block()

    node.close()
    node.block_log.copy_to(args.output_block_log_directory)
    chain_parameters.export_to_file(args.output_block_log_directory)

    tt.logger.info(f"Block_log created successfully. Save in {args.output_block_log_directory} Exiting...")


def import_keys(wallet: tt.Wallet, account_name: str) -> None:
    owner = tt.Account(account_name, secret="owner").private_key
    active = tt.Account(account_name, secret="active").private_key
    posting = tt.Account(account_name, secret="posting").private_key
    wallet.api.import_keys([owner, active, posting])


def get_absolute_slot_at_time(date_string: str) -> int:
    date_object = datetime.datetime.strptime(date_string, "%Y-%m-%d %H:%M:%S")
    gmt_timezone = pytz.timezone("GMT")
    gmt_datetime = date_object.replace(tzinfo=pytz.utc).astimezone(gmt_timezone)
    block = int(gmt_datetime.timestamp()) - HIVE_GENESIS_TIME
    return int(block / 3)


if __name__ == "__main__":
    generate_blocklog_for_denser(args.input_block_log_directory)
