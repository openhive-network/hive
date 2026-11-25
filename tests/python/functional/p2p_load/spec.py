"""What a harness run measures: the spec the host side writes and the driver reads."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from typing import Any

TOPOLOGIES = ("mesh", "star", "strict", "chain", "diamond")
MIXES = ("default", "large")
# where a two-host run's nodes run: "local" with the witness and the run's command, "remote" on --remote-host
PARTS = ("local", "remote")
# what a hived option names besides a single role: the roles it covers, by prefix, and how specific it is (a role is
# more specific than either); the most specific option for a key wins on a node
OPTION_GROUPS = {"all": ("", 0), "colonies": ("colony", 1), "relays": ("relay", 1)}
_ROLE_SPECIFICITY = 2
_OPTION_KEY = re.compile(r"^[a-z0-9][a-z0-9-]*$")
# set by the harness itself: the p2p parameters through their own options, the endpoints and seeds by the topology
_HARNESS_OPTIONS = {"p2p-parameters": "--p2p-parameter", "p2p-endpoint": "--topology", "p2p-seed-node": "--topology"}
# oversized transactions: larger than a peer's send queue holds (GRAPHENE_NET_MAXIMUM_QUEUED_MESSAGES_IN_BYTES) and at
# most what a block of the largest maximum_block_size (HIVE_MAX_BLOCK_SIZE) admits, which is 256 bytes less
SEND_QUEUE_BYTES = 1024 * 1024
OVERSIZED_TX_MAX_BYTES = 2 * 1024 * 1024 - 256
OVERSIZED_TX_DEFAULT_BYTES = 1_400_000
# the first goes out this many loaded blocks after the colonies start, once the flooded pending has built up, the next
# ones as many blocks apart; each gets as many blocks again before the run's end to reach one
OVERSIZED_TX_FIRST_BLOCK = 5
OVERSIZED_TX_EVERY_BLOCKS = 3
OVERSIZED_TX_INCLUSION_BLOCKS = 10


@dataclass
class RunSpec:
    """What `harness.py` asks the driver to run (written as spec.json next to out.json)."""

    name: str
    host: str
    label: str
    out_dir: str
    block_log: str
    hived: str
    tpb: int
    blocks: int
    topology: str
    colonies: int = 1
    relays: int = 0
    node_hived: dict[str, str] = field(default_factory=dict)
    p2p_parameters: dict[str, Any] = field(default_factory=dict)
    late_observer_blocks: int = 0
    colony_ports: dict[str, int] = field(default_factory=dict)
    netem_ms: int = 0
    colony_start_delay: int = 12
    mix: str = "default"
    # per colony index: another mix than `mix`, and how many blocks after the others' start it starts
    colony_mixes: dict[str, str] = field(default_factory=dict)
    colony_late_blocks: dict[str, int] = field(default_factory=dict)
    p2p_loggers: bool = True
    replay_templates: str = ""  # replayed-state template directory; "" replays the block log on every node
    remote_host: str = ""
    remote_roles: list[str] = field(default_factory=list)
    # two-host runs: the address:port each listening node is dialed at, on its host's network address
    endpoints: dict[str, str] = field(default_factory=dict)
    # [selector, key, value]: a hived config option for one role or a group of OPTION_GROUPS, in the given order
    hived_options: list[list[str]] = field(default_factory=list)
    # signed transactions over SEND_QUEUE_BYTES a local colony broadcasts during the loaded window, and their size
    oversized_txs: int = 0
    oversized_tx_bytes: int = OVERSIZED_TX_DEFAULT_BYTES

    def roles(self) -> list[str]:
        return [
            "witness",
            "observer",
            *(f"colony{i}" for i in range(self.colonies)),
            *(f"relay{i + 1}" for i in range(self.relays)),
        ]

    def roles_on(self, part: str) -> list[str]:
        """The roles whose nodes run on `part`'s host ("local" or "remote")."""
        return [role for role in self.roles() if (role in self.remote_roles) == (part == "remote")]

    def oversized_tx_blocks(self) -> list[int]:
        """The loaded blocks, counted from the colonies' start, at which each oversized transaction is broadcast."""
        return [OVERSIZED_TX_FIRST_BLOCK + i * OVERSIZED_TX_EVERY_BLOCKS for i in range(self.oversized_txs)]

    def oversized_tx_colony(self) -> str:
        """The colony that broadcasts the oversized transactions: the first on the witness's host."""
        return next(role for role in self.roles_on("local") if role.startswith("colony"))

    def binary(self, role: str) -> str:
        return self.node_hived.get(role, self.hived)

    @staticmethod
    def _selects(selector: str, role: str) -> bool:
        return role.startswith(OPTION_GROUPS[selector][0]) if selector in OPTION_GROUPS else selector == role

    def hived_options_of(self, role: str) -> list[tuple[str, str]]:
        """
        The hived options of `role`'s node, as (key, value): for each key, and for each logger of `log-logger`, the
        value of the most specific selector naming the node (the role, then its group, then `all`).

        Raise ValueError when two options of one specificity set the same key (or logger) on the node.
        """
        chosen: dict[str, tuple[int, str, str, str]] = {}
        for selector, key, value in self.hived_options:
            if not self._selects(selector, role):
                continue
            rank = OPTION_GROUPS[selector][1] if selector in OPTION_GROUPS else _ROLE_SPECIFICITY
            slot = f"log-logger {json.loads(value)['name']}" if key == "log-logger" else key
            if slot in chosen and chosen[slot][0] == rank:
                raise ValueError(f"{chosen[slot][1]} and {selector} both set {slot} on {role}")
            if slot not in chosen or chosen[slot][0] < rank:
                chosen[slot] = (rank, selector, key, value)
        return [(key, value) for _, _, key, value in chosen.values()]

    def validate(self) -> None:
        """Raise ValueError for a spec the driver cannot build."""
        if self.topology not in TOPOLOGIES:
            raise ValueError(f"topology must be one of {', '.join(TOPOLOGIES)}, not {self.topology!r}")
        if self.topology not in {"chain", "diamond"} and self.relays:
            raise ValueError("relays exist only in the chain and diamond topologies")
        if self.topology == "diamond" and self.relays < 2:
            raise ValueError("the diamond topology needs at least two relays")
        if self.tpb <= 0 or self.blocks <= 0 or self.colonies <= 0 or self.relays < 0:
            raise ValueError("tx per block, blocks and colonies must be positive, relays not negative")
        if self.late_observer_blocks < 0 or self.netem_ms < 0:
            raise ValueError("late observer blocks and netem delay must not be negative")
        unknown = set(self.node_hived) - set(self.roles())
        if unknown:
            raise ValueError(f"per-node binaries for roles this run does not have: {sorted(unknown)}")
        if any(int(i) >= self.colonies for i in self.colony_ports):
            raise ValueError("netem colony index beyond the number of colonies")
        if self.late_observer_blocks >= self.blocks:
            raise ValueError("the late observer must join before the last measured block")
        self._validate_colonies()
        self._validate_placement()
        self._validate_hived_options()
        self._validate_oversized_txs()

    def _validate_colonies(self) -> None:
        for mix in [self.mix, *self.colony_mixes.values()]:
            if mix not in MIXES:
                raise ValueError(f"mix must be one of {', '.join(MIXES)}, not {mix!r}")
        if any(int(i) >= self.colonies for i in [*self.colony_mixes, *self.colony_late_blocks]):
            raise ValueError("per-colony mix or start for a colony index beyond the number of colonies")
        if any(blocks < 0 or blocks >= self.blocks for blocks in self.colony_late_blocks.values()):
            raise ValueError("a late colony must start before the last measured block")

    def _validate_placement(self) -> None:
        if bool(self.remote_host) != bool(self.remote_roles):
            raise ValueError("a remote host needs remote roles, and remote roles a remote host")
        if not self.remote_host:
            return
        unknown = set(self.remote_roles) - set(self.roles())
        if unknown:
            raise ValueError(f"remote roles this run does not have: {sorted(unknown)}")
        if "witness" in self.remote_roles:
            raise ValueError("the witness runs on the local host: it measures the run")
        if self.netem_ms:
            raise ValueError("netem delays loopback links; a two-host run's links cross the network")
        if self.late_observer_blocks and "observer" in self.remote_roles:
            raise ValueError("a late observer must run on the local host")
        missing = {role for role in self.roles() if role != "observer"} - set(self.endpoints)
        if missing:
            raise ValueError(f"no p2p endpoint for {sorted(missing)}")

    def _validate_oversized_txs(self) -> None:
        if self.oversized_txs < 0:
            raise ValueError("the number of oversized transactions must not be negative")
        if not self.oversized_txs:
            return
        if not SEND_QUEUE_BYTES < self.oversized_tx_bytes <= OVERSIZED_TX_MAX_BYTES:
            raise ValueError(
                f"an oversized transaction takes more than {SEND_QUEUE_BYTES} and at most {OVERSIZED_TX_MAX_BYTES} "
                f"bytes, not {self.oversized_tx_bytes}"
            )
        last = self.oversized_tx_blocks()[-1] + OVERSIZED_TX_INCLUSION_BLOCKS
        if last > self.blocks:
            raise ValueError(
                f"{self.oversized_txs} oversized transactions need {last} loaded blocks, the run has {self.blocks}"
            )
        if not any(role.startswith("colony") for role in self.roles_on("local")):
            raise ValueError("oversized transactions go out from a colony on the local host; this run has none there")

    def _validate_hived_options(self) -> None:
        for selector, key, value in self.hived_options:
            if selector not in OPTION_GROUPS and selector not in self.roles():
                raise ValueError(
                    f"hived option for {selector!r}: neither a role of this run nor {', '.join(OPTION_GROUPS)}"
                )
            if not any(self._selects(selector, role) for role in self.roles()):
                raise ValueError(f"hived option for {selector}: this run has none")
            _validate_hived_option(key, value)
        for role in self.roles():
            self.hived_options_of(role)


def _validate_hived_option(key: str, value: str) -> None:
    if not _OPTION_KEY.match(key):
        raise ValueError(f"hived option {key!r} is not an option name")
    if key in _HARNESS_OPTIONS:
        raise ValueError(f"hived option {key} is the harness's: use {_HARNESS_OPTIONS[key]}")
    # one config.ini line each: a line break would add options nobody checks
    if not value or "\n" in value or "\r" in value:
        raise ValueError(f"hived option {key}: the value must be one non-empty line")
    if key == "log-logger":
        try:
            logger = json.loads(value)
        except json.JSONDecodeError as error:
            raise ValueError(f"hived option log-logger: {value!r} is not JSON") from error
        if not (isinstance(logger, dict) and isinstance(logger.get("name"), str)):
            raise ValueError("hived option log-logger: the value must be one logger object with a name")
