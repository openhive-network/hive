"""Who dials whom in a run's topology, by role, so that each host of a two-host run wires only its own nodes."""

from __future__ import annotations

import itertools
from dataclasses import dataclass, field
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from python.functional.p2p_load.spec import RunSpec


@dataclass
class Wiring:
    seeds: dict[str, list[str]] = field(default_factory=dict)  # role -> the roles it dials at start
    max_connections: dict[str, int] = field(default_factory=dict)  # p2p-max-connections, where limited

    def seed(self, role: str, *peers: str) -> None:
        self.seeds[role] = list(peers)

    @property
    def links(self) -> list[tuple[str, str]]:
        """(role, a seed of it), in wiring order."""
        return [(role, peer) for role, peers in self.seeds.items() for peer in peers]


def _mesh(w: Wiring, colonies: list[str], relays: list[str]) -> None:
    for role in ["observer", *colonies]:
        w.seed(role, "witness")


def _star(w: Wiring, colonies: list[str], relays: list[str]) -> None:
    w.seed("witness", *colonies)
    w.seed("observer", "witness")


def _strict(w: Wiring, colonies: list[str], relays: list[str]) -> None:
    _star(w, colonies, relays)
    for role in ["observer", *colonies]:
        w.max_connections[role] = 1


def _chain(w: Wiring, colonies: list[str], relays: list[str]) -> None:
    # colony0 -> relay1 -> ... -> relayN -> witness -> observer; the connection limits leave peer exchange
    # no room for a shortcut
    for previous, following in itertools.pairwise([colonies[0], *relays, "witness"]):
        w.seed(following, previous)
    w.max_connections[colonies[0]] = 1
    for role in [*relays, "witness"]:
        w.max_connections[role] = 2
    w.seed("observer", "witness")
    w.max_connections["observer"] = 1


_TOPOLOGIES = {"mesh": _mesh, "star": _star, "strict": _strict, "chain": _chain}


def wire(spec: RunSpec) -> Wiring:
    wiring = Wiring()
    colonies = [f"colony{i}" for i in range(spec.colonies)]
    relays = [f"relay{i + 1}" for i in range(spec.relays)]
    _TOPOLOGIES[spec.topology](wiring, colonies, relays)
    return wiring
