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

    def start_waves(self, roles: list[str]) -> list[list[str]]:
        """Group `roles` into start waves, each one wave after the latest of its seeds (the topologies have no cycle)."""
        wave: dict[str, int] = {}

        def wave_of(role: str) -> int:
            if role not in wave:
                seeds = [seed for seed in self.seeds.get(role, []) if seed in roles]
                wave[role] = 1 + max((wave_of(seed) for seed in seeds), default=-1)
            return wave[role]

        waves: list[list[str]] = [[] for _ in range(1 + max(map(wave_of, roles)))]
        for role in roles:
            waves[wave_of(role)].append(role)
        return waves


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
    # every colony -> relay1 -> ... -> relayN -> witness -> observer; the connection limits leave peer exchange
    # no room for a shortcut
    hops = [*relays, "witness"]
    w.seed(hops[0], *colonies)
    for previous, following in itertools.pairwise(hops):
        w.seed(following, previous)
    for colony in colonies:
        w.max_connections[colony] = 1
    for role in hops:
        w.max_connections[role] = 2
    w.max_connections[hops[0]] = len(colonies) + 1
    w.seed("observer", "witness")
    w.max_connections["observer"] = 1


def _diamond(w: Wiring, colonies: list[str], relays: list[str]) -> None:
    # a strict colony star whose witness reaches the observer only through every relay, so each block reaches the
    # observer from as many advertisers as there are relays; a relay has room for one other relay besides the
    # witness and the observer, in case peer exchange links two of them before the observer dials in
    _strict(w, colonies, relays)
    for relay in relays:
        w.seed(relay, "witness")
        w.max_connections[relay] = 3
    w.seed("observer", *relays)
    w.max_connections["observer"] = len(relays)


_TOPOLOGIES = {"mesh": _mesh, "star": _star, "strict": _strict, "chain": _chain, "diamond": _diamond}


def wire(spec: RunSpec) -> Wiring:
    wiring = Wiring()
    colonies = [f"colony{i}" for i in range(spec.colonies)]
    relays = [f"relay{i + 1}" for i in range(spec.relays)]
    _TOPOLOGIES[spec.topology](wiring, colonies, relays)
    return wiring
