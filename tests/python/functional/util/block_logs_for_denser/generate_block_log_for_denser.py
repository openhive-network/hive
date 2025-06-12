from __future__ import annotations

import datetime
from pathlib import Path

import pytz
from denser_block_log_config import ACCOUNT_DETAILS, COMMUNITY_DETAILS

import test_tools as tt
from hive_local_tools.constants import (
    HIVE_GENESIS_TIME,
    MIRRORNET_5M_WITNESSES,
    MIRRORNET_SKELETON_KEY,
)
from hive_local_tools.functional import wait_for_current_hardfork
from hive_local_tools.functional.python.block_log_generation import parse_block_log_generator_args
from hive_local_tools.functional.python.operation import (
    create_transaction_with_any_operation,
)
from schemas.operations.custom_json_operation import CustomJsonOperation


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

    chain_parameters = set_chain_spec(block_log, current_hardfork_number)

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

    wallet = tt.Wallet(attach_to=node)

    create_accounts(wallet, ACCOUNT_DETAILS)
    fund_accounts(wallet, ACCOUNT_DETAILS)

    create_posts(node, wallet, 40)

    # create communities
    communities = []
    for details in COMMUNITY_DETAILS:
        communities.append(
            Community(wallet, community_creator="blocktrades", community_name=details.owner, details=details)
        )
        communities[-1].create()

    # update communities properties
    for community in communities:
        community.update_props(
            title=community.details.title,
            about=community.details.about,
            is_nsfw=community.details.is_nsfw,
            description=community.details.description,
            flag_text=community.details.flag_text,
        )

    # create posts connected with communities
    for community in communities:
        community.create_posts(node, author="blocktrades", pined_posts=community.details.pinned_posts)

    # subscribe
    for community in communities:
        community.subscribe(community.details.subs)

    # set roles
    for community in communities:
        community.set_role(community.details.roles)

    node.wait_for_irreversible_block()
    wallet.close()
    node.close()
    node.block_log.copy_to(args.output_block_log_directory)
    chain_parameters.export_to_file(args.output_block_log_directory)

    tt.logger.info(f"Block_log created successfully. Save in {args.output_block_log_directory} Exiting...")


def get_absolute_slot_at_time(date_string: str) -> int:
    """
    Function to calculate blocks number, including missing blocks
    """
    date_object = datetime.datetime.strptime(date_string, "%Y-%m-%d %H:%M:%S")
    gmt_timezone = pytz.timezone("GMT")
    gmt_datetime = date_object.replace(tzinfo=pytz.utc).astimezone(gmt_timezone)
    block = int(gmt_datetime.timestamp()) - HIVE_GENESIS_TIME
    return int(block / 3)


def set_chain_spec(block_log: tt.BlockLog, current_hardfork_number: int):
    return tt.AlternateChainSpecs(
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


def create_accounts(wallet: tt.Wallet, details: list) -> None:
    for account_name, _ in details:
        posting = tt.Account(account_name, secret="posting")
        active = tt.Account(account_name, secret="active")
        owner = tt.Account(account_name, secret="owner")
        memo = tt.Account(account_name, secret="memo")

        wallet.api.import_keys([posting.private_key, active.private_key, owner.private_key, memo.private_key])

        wallet.api.create_account_with_keys(
            "blocktrades",
            account_name,
            "{}",
            posting=posting.public_key,
            active=active.public_key,
            owner=owner.public_key,
            memo=memo.public_key,
        )

        save_keys_to_file(account_name, posting, active, owner, memo)


def save_keys_to_file(
    account_name: str, posting: tt.Account, active: tt.Account, owner: tt.Account, memo: tt.Account
) -> None:
    with open(args.output_block_log_directory.joinpath("account_specification.txt"), "a", encoding="utf-8") as file:
        file.write(account_name.center(70, "-") + "\n")
        for key in [posting, active, owner, memo]:
            file.write(f"{key.secret}-private: {key.private_key}\n{key.secret}-public: {key.public_key}\n")
        file.write(70 * "-" + "\n\n")


def fund_accounts(wallet: tt.Wallet, details: list) -> None:
    for account, ops in details:
        if "vesting" in ops:
            wallet.api.transfer_to_vesting("blocktrades", account, ops["vesting"])
        if "transfer" in ops:
            for asset in ops["transfer"]:
                wallet.api.transfer("blocktrades", account, asset, f"transfer-{asset.pretty_amount()}")


def create_posts(node: tt.InitNode, wallet: tt.Wallet, posts_number: int) -> None:
    for post_num in range(posts_number):
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


class Community:
    def __init__(self, wallet, community_creator, community_name, details):
        self.wallet = wallet
        self.community_creator = community_creator
        self.community_name = community_name
        self.details = details

    def create(self):
        owner = tt.Account(self.community_name, secret="owner")
        active = tt.Account(self.community_name, secret="active")
        posting = tt.Account(self.community_name, secret="posting")
        memo = tt.Account(self.community_name, secret="memo")

        self.wallet.api.import_keys([owner.private_key, active.private_key, posting.private_key, memo.private_key])

        self.wallet.api.create_account_with_keys(
            self.community_creator,
            self.community_name,
            "{}",
            owner.public_key,
            active.public_key,
            posting.public_key,
            memo.public_key,
        )

    def __send_custom_json(self, json: str, required_auths=None, required_posting_auths=None):
        create_transaction_with_any_operation(
            self.wallet,
            [
                CustomJsonOperation(
                    required_auths=required_auths if required_auths else [],
                    required_posting_auths=required_posting_auths if required_posting_auths else [],
                    id_="community",
                    json_=str(json),
                )
            ],
            broadcast=True,
        )

    def update_props(self, title: str, about: str, is_nsfw: bool, description: str, flag_text: str) -> None:
        json = (
            f'["updateProps",{{"community":"{self.community_name}","props":{{"title":"{title}","about":"{about}",'
            f'"is_nsfw":{str(is_nsfw).lower()},"description":"{description}","flag_text":"{flag_text}"}}}}]'
        )
        self.__send_custom_json(json, required_posting_auths=[self.community_name])
        tt.logger.info(f"Update props in community {self.community_name}")

    def set_role(self, roles) -> None:
        for role in ["admin", "mod", "guest", "member", "muted"]:
            if hasattr(roles, role) and getattr(roles, role) is not None:
                for acc in getattr(roles, role):
                    json = (
                        f'["setRole", {{"community": "{self.community_name}", "account": "{acc}", "role": "{role}"}}]'
                    )
                    self.__send_custom_json(json, required_posting_auths=[self.community_name])
                    tt.logger.info(f"Set `{acc}` role to `{role}` in {self.community_name}")

    def subscribe(self, accounts: list[str]) -> None:
        for account in accounts:
            json = f'["subscribe",{{"community":"{self.community_name}"}}]'
            self.__send_custom_json(json, required_posting_auths=[account])
            tt.logger.info(f"account `{account}` subscribe community `{self.community_name}`")

    def pin_post(self, account: str, permlink: str) -> None:
        json = f'["pinPost",{{"community":"{self.community_name}","account":"{account}","permlink":"{permlink}"}}]'
        self.__send_custom_json(json, required_posting_auths=[self.community_name])
        tt.logger.info(f"Pin post `{permlink}` in community: {self.community_name}")

    def create_posts(self, node: tt.InitNode, author: str, pined_posts: int) -> None:
        for post_num in range(self.details.posts_number):
            permlink = f"permlink-community-{self.community_name}-post-test-{post_num}"
            self.wallet.api.post_comment(
                author=author,
                permlink=permlink,
                parent_author="",
                parent_permlink=self.community_name,
                title=f"test-post-number-{post_num}-for-community-{self.community_name}",
                body=f"body of test post {post_num} numer for community {self.community_name}",
                json="{}",
            )
            node.wait_number_of_blocks(1)
            tt.logger.info(
                f"Created post number: {post_num + 1}/{self.details.posts_number}. For community {self.community_name}"
            )

            assert self.details.posts_number >= pined_posts, "you can't pin more posts than you create"
            if post_num < pined_posts:
                self.pin_post(author, permlink)


if __name__ == "__main__":
    args = parse_block_log_generator_args()
    generate_blocklog_for_denser(args.input_block_log_directory)
