"""What a harness run measures: the spec the host side writes and the driver reads."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

TOPOLOGIES = ("mesh", "star", "strict", "chain")


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
    p2p_loggers: bool = True
    replay_templates: str = ""  # replayed-state template directory; "" replays the block log on every node

    def roles(self) -> list[str]:
        return [
            "witness",
            "observer",
            *(f"colony{i}" for i in range(self.colonies)),
            *(f"relay{i + 1}" for i in range(self.relays)),
        ]

    def binary(self, role: str) -> str:
        return self.node_hived.get(role, self.hived)

    def validate(self) -> None:
        """Raise ValueError for a spec the driver cannot build."""
        if self.topology not in TOPOLOGIES:
            raise ValueError(f"topology must be one of {', '.join(TOPOLOGIES)}, not {self.topology!r}")
        if self.topology == "chain" and self.colonies != 1:
            raise ValueError("the chain topology has exactly one colony")
        if self.topology != "chain" and self.relays:
            raise ValueError("relays exist only in the chain topology")
        if self.tpb <= 0 or self.blocks <= 0 or self.colonies <= 0 or self.relays < 0:
            raise ValueError("tx per block, blocks and colonies must be positive, relays not negative")
        if self.late_observer_blocks < 0 or self.netem_ms < 0:
            raise ValueError("late observer blocks and netem delay must not be negative")
        if self.mix not in {"default", "large"}:
            raise ValueError(f"mix must be default or large, not {self.mix!r}")
        unknown = set(self.node_hived) - set(self.roles())
        if unknown:
            raise ValueError(f"per-node binaries for roles this run does not have: {sorted(unknown)}")
        if any(int(i) >= self.colonies for i in self.colony_ports):
            raise ValueError("netem colony index beyond the number of colonies")
        if self.late_observer_blocks >= self.blocks:
            raise ValueError("the late observer must join before the last measured block")
