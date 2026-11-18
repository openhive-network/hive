"""
One harness run inside the test-tools environment: build the topology, replay, load it with colony, sample, record.

Runs in the container `harness.py` starts, with the run's tmpfs directory as working directory, so test-tools puts
every node's data under it. Writes roles.json as soon as the nodes exist and out.json when the run ends.
"""

from __future__ import annotations

import itertools
import json
import os
import socket
import threading
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from dataclasses import asdict, dataclass, field
from datetime import timedelta
from pathlib import Path
from typing import Any, TypeAlias

from beekeepy.exceptions import CommunicationError
from beekeepy.interfaces import P2PUrl

import test_tools as tt
from python.functional.p2p_load import p2p_loggers, p2p_parameters, procfs
from python.functional.p2p_load.spec import RunSpec
from test_tools import complex_networks as ttcn

_SAMPLE_INTERVAL_S = 3
_LINKS_EVERY_N_SAMPLES = 10
_NODE_TIMEOUT_S = 300
# a node dials its seeds once at start and, refused, only after the p2p retry backoff (60-120 s); the driver redials
# them until every link is up, this long at most
_SEED_LINK_TIMEOUT_S = 60
_SEED_LINK_POLL_S = 1
# The colony tests' setup (functional/colony), repeated here: the driver runs with the test-tools of the image, which
# hive_local_tools' helpers may have outgrown, so it imports nothing from the functional tests.
_WITNESSES = [f"witness-{w}" for w in range(20)]
_COLONY_KEY = "5JU5vHLxvs6RhAmUnyZHzdu6DpiJ9cbpkkqKWkD3GDpser9Qa6p"  # the block log's workers' single authority
_MIXES = {
    "default": {  # the hive#232 mix
        "colony_article": '{"min": 100, "max": 5000, "weight": 16, "exponent": 4}',
        "colony_reply": '{"min": 30, "max": 1000, "weight": 110, "exponent": 5}',
        "colony_vote": '{"weight": 2070}',
        "colony_transfer": '{"min": 0, "max": 350, "weight": 87, "exponent": 4}',
        "colony_custom": '{"min": 20, "max": 400, "weight": 6006, "exponent": 1}',
    },
    "large": {  # replies and articles only, 4000..12000 byte bodies (about 255 transactions fill a 2 MiB block)
        "colony_article": '{"min": 4000, "max": 12000, "weight": 1, "exponent": 1}',
        "colony_reply": '{"min": 4000, "max": 12000, "weight": 4, "exponent": 1}',
        "colony_vote": '{"weight": 0}',
        "colony_transfer": '{"min": 0, "max": 0, "weight": 0, "exponent": 1}',
        "colony_custom": '{"min": 0, "max": 0, "weight": 0, "exponent": 1}',
    },
}

_Node: TypeAlias = tt.WitnessNode | tt.ApiNode


def _create_node(spec: RunSpec, role: str, colony_start_block: int) -> _Node:
    # test-tools reads the hived path when a node is created, so this picks the node's binary
    tt.paths_to_executables.set_path_of("hived", Path(spec.binary(role)))
    if role == "witness":
        witness = tt.WitnessNode(witnesses=["initminer", *_WITNESSES])
        witness.config.shared_file_size = "4G"
        for name in [*_WITNESSES, "initminer"]:
            witness.config.private_key.append(tt.Account(name).private_key)
        return witness
    node = tt.ApiNode()
    node.config.shared_file_size = "4G"
    if role.startswith("colony"):
        node.config.plugin.append("colony")
        node.config.colony_sign_with = [_COLONY_KEY]
        for option, value in _MIXES[spec.mix].items():
            setattr(node.config, option, value)
        node.config.colony_transactions_per_block = spec.tpb // spec.colonies
        node.config.colony_start_at_block = colony_start_block
    return node


@dataclass
class _Topology:
    witness: _Node
    observer: _Node
    colonies: list[_Node]
    relays: list[_Node]
    endpoint: dict[int, P2PUrl]  # id(node) -> its p2p endpoint
    links: list[tuple[_Node, _Node]] = field(default_factory=list)  # (node, a seed of it)

    def seed(self, node: _Node, *peers: _Node) -> None:
        node.config.p2p_seed_node = [self.endpoint[id(peer)] for peer in peers]
        self.links.extend((node, peer) for peer in peers)

    def address(self, node: _Node) -> str | None:
        """host:port the node listens on for p2p, as hived names its peers; None for a node that does not listen."""
        endpoint = self.endpoint.get(id(node))
        return None if endpoint is None else endpoint.as_string(with_protocol=False)


def _connect_mesh(t: _Topology) -> None:
    for node in [t.observer, *t.colonies]:
        t.seed(node, t.witness)


def _connect_star(t: _Topology) -> None:
    t.seed(t.witness, *t.colonies)
    t.seed(t.observer, t.witness)


def _connect_strict(t: _Topology) -> None:
    _connect_star(t)
    for node in [t.observer, *t.colonies]:
        node.config.p2p_max_connections = 1


def _connect_chain(t: _Topology) -> None:
    # colony0 -> relay1 -> ... -> relayN -> witness -> observer; the connection limits leave peer exchange
    # no room for a shortcut
    for previous, following in itertools.pairwise([t.colonies[0], *t.relays, t.witness]):
        t.seed(following, previous)
    t.colonies[0].config.p2p_max_connections = 1
    for node in [*t.relays, t.witness]:
        node.config.p2p_max_connections = 2
    t.seed(t.observer, t.witness)
    t.observer.config.p2p_max_connections = 1


_TOPOLOGIES = {"mesh": _connect_mesh, "star": _connect_star, "strict": _connect_strict, "chain": _connect_chain}


def _connect(spec: RunSpec, nodes: dict[str, _Node]) -> _Topology:
    free = iter(ttcn.generate_free_addresses(len(nodes)))
    endpoint: dict[int, P2PUrl] = {}
    for role, node in nodes.items():
        if role == "observer":
            continue
        port = spec.colony_ports.get(role.removeprefix("colony")) if role.startswith("colony") else None
        # a netem-delayed colony listens on the port the host side filters on
        endpoint[id(node)] = P2PUrl(f"127.0.0.1:{port}" if port else next(free))
        node.config.p2p_endpoint = endpoint[id(node)]
    topology = _Topology(
        witness=nodes["witness"],
        observer=nodes["observer"],
        colonies=[nodes[f"colony{i}"] for i in range(spec.colonies)],
        relays=[nodes[f"relay{i + 1}"] for i in range(spec.relays)],
        endpoint=endpoint,
    )
    _TOPOLOGIES[spec.topology](topology)
    return topology


def _peer_hosts(node: _Node) -> set[str]:
    return {peer.host for peer in node.api.network_node.get_connected_peers().connected_peers}


def _add_node(node: _Node, endpoint: str) -> None:
    """
    Have `node` dial `endpoint` now, resetting its p2p retry backoff for it.

    Sent as plain JSON-RPC: the image's test-tools expects an `endpoint` field in network_node_api.add_node's
    result, which hived returns empty, and fails on every call.
    """
    request = {"jsonrpc": "2.0", "id": 1, "method": "network_node_api.add_node", "params": {"endpoint": endpoint}}
    with urllib.request.urlopen(
        urllib.request.Request(
            str(node.http_endpoint),
            data=json.dumps(request).encode(),
            headers={"Content-Type": "application/json"},
        ),
        timeout=10,
    ) as response:
        result = json.loads(response.read())
    if "error" in result:
        raise RuntimeError(f"{node}: network_node_api.add_node({endpoint}) failed: {result['error']}")


def _wait_for_seed_links(topology: _Topology, nodes: list[_Node]) -> float:
    """
    Redial the seeds of `nodes` until each node is linked to every seed of it; return how long that took.

    A link is up when either end lists the other's p2p endpoint as a peer: hived keeps one connection per pair of
    nodes, so the seed may hold the connection it dialed itself. Raise RuntimeError naming the links still down
    after _SEED_LINK_TIMEOUT_S.
    """
    running = {id(node) for node in nodes}
    links = [(node, seed) for node, seed in topology.links if id(node) in running and id(seed) in running]
    started = time.time()
    while True:
        hosts = {id(node): _peer_hosts(node) for node in nodes}
        down = [
            (node, seed)
            for node, seed in links
            if topology.address(seed) not in hosts[id(node)] and topology.address(node) not in hosts[id(seed)]
        ]
        if not down:
            return time.time() - started
        if time.time() - started > _SEED_LINK_TIMEOUT_S:
            names = ", ".join(f"{node} -> {seed}" for node, seed in down)
            raise RuntimeError(f"p2p links still down {_SEED_LINK_TIMEOUT_S} s after the nodes started: {names}")
        for node, seed in down:
            _add_node(node, str(topology.address(seed)))
        time.sleep(_SEED_LINK_POLL_S)


def _apply_p2p_parameters(spec: RunSpec, nodes: dict[str, _Node]) -> dict[str, Any]:
    defaults = {path: p2p_parameters.binary_defaults(Path(path)) for path in {spec.binary(r) for r in nodes}}
    applied: dict[str, dict[str, Any]] = {}
    unsupported: dict[str, list[str]] = {}
    for role, node in nodes.items():
        selection = p2p_parameters.select(spec.p2p_parameters, defaults[spec.binary(role)])
        applied[role] = selection.applied
        if selection.unsupported:
            unsupported[role] = selection.unsupported
            tt.logger.warning(f"{role}: {spec.binary(role)} has no p2p parameter {selection.unsupported}; left out")
        if selection.applied:
            node.config.p2p_parameters = json.dumps(selection.applied)
    return {"given": spec.p2p_parameters, "applied": applied, "unsupported": unsupported, "binary_defaults": defaults}


def _enable_p2p_loggers(spec: RunSpec, nodes: dict[str, _Node]) -> dict[str, list[str]]:
    """Enable the opt-in p2p loggers (unless the spec turns them off) on every node whose binary has them."""
    binaries = {spec.binary(r) for r in nodes}
    available = {path: p2p_loggers.binary_loggers(Path(path)) if spec.p2p_loggers else [] for path in binaries}
    enabled: dict[str, list[str]] = {}
    for role, node in nodes.items():
        enabled[role] = available[spec.binary(role)]
        if enabled[role]:
            node.config.log_logger = p2p_loggers.with_loggers(node.config.log_logger, enabled[role])
    return enabled


def _write_json(path: Path, data: Any) -> None:
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data))
    tmp.replace(path)


class _Sampler(threading.Thread):
    """Every few seconds: the witness head and every node's per-thread CPU ticks; now and then the p2p link map."""

    def __init__(self, witness: _Node, pids: dict[str, int]) -> None:
        super().__init__(daemon=True)
        self._witness = witness
        self.pids = pids
        self.samples: list[dict[str, Any]] = []
        self.links: list[dict[str, Any]] = []
        self._stop_event = threading.Event()

    def run(self) -> None:
        while not self._stop_event.is_set():
            pids = dict(self.pids)
            try:
                head = self._witness.get_last_block_number()
                ticks = {role: procfs.thread_ticks(pid) for role, pid in pids.items()}
                self.samples.append({"t": time.time(), "head": head, "ticks": ticks})
                if len(self.samples) % _LINKS_EVERY_N_SAMPLES == 1:
                    self.links.append({"head": head, "links": procfs.tcp_links(pids)})
            except (OSError, CommunicationError) as error:  # a missed sample must not abort the measured run
                tt.logger.warning(f"sampler: {error!r}")
            self._stop_event.wait(_SAMPLE_INTERVAL_S)

    def stop(self) -> None:
        self._stop_event.set()
        self.join()


def _pid_of(node: _Node) -> int:
    pid = procfs.hived_pid(node.directory)
    if pid is None:
        raise RuntimeError(f"no hived process serves {node.directory}")
    return pid


def run(spec: RunSpec) -> dict[str, Any]:
    out_dir = Path(spec.out_dir)
    block_log = tt.BlockLog(Path(spec.block_log), "auto")
    chain_specs = tt.AlternateChainSpecs.parse_file(Path(spec.block_log) / tt.AlternateChainSpecs.FILENAME)
    colony_start_block = block_log.get_head_block_number() + spec.colony_start_delay

    nodes = {role: _create_node(spec, role, colony_start_block) for role in spec.roles()}
    topology = _connect(spec, nodes)
    parameters = _apply_p2p_parameters(spec, nodes)
    loggers = _enable_p2p_loggers(spec, nodes)
    roles = {role: node.directory.name for role, node in nodes.items()}
    _write_json(
        out_dir / "roles.json", {"roles": roles, "directories": {r: str(n.directory) for r, n in nodes.items()}}
    )

    for node in nodes.values():
        node.run(
            replay_from=block_log,
            timeout=_NODE_TIMEOUT_S,
            alternate_chain_specs=chain_specs,
            exit_before_synchronization=True,
        )
    for role, node in nodes.items():
        p2p_parameters.verify_config(node.config_file_path, parameters["applied"][role])
        p2p_loggers.verify_config(node.config_file_path, loggers[role])

    late = spec.late_observer_blocks > 0
    start_time = block_log.get_head_block_time()
    started = time.time()
    starting = [node for role, node in nodes.items() if not (late and role == "observer")]
    with ThreadPoolExecutor(max_workers=len(starting)) as pool:
        for future in [
            pool.submit(
                node.run,
                timeout=_NODE_TIMEOUT_S,
                wait_for_live=False,
                alternate_chain_specs=chain_specs,
                time_control=tt.StartTimeControl(start_time=start_time),
            )
            for node in starting
        ]:
            future.result()
    # started together, a node may dial a seed that does not listen yet; the colonies must not wait out the backoff
    seed_links_s = _wait_for_seed_links(topology, starting)
    for node in starting:
        node.wait_for_live_mode(timeout=_NODE_TIMEOUT_S)
    witness = nodes["witness"]
    pids = {role: _pid_of(node) for role, node in nodes.items() if not (late and role == "observer")}
    first = witness.get_last_block_number()
    sampler = _Sampler(witness, pids)
    sampler.start()

    late_observer: dict[str, Any] | None = None
    if late:
        join_block = colony_start_block + spec.late_observer_blocks
        witness.wait_for_block_with_number(join_block, timeout=(join_block - first) * 3 * 4 + 120)
        joined_at = witness.get_last_block_number()
        joining = time.time()
        # the others' faked clocks started at start_time when they were started; the observer's starts where theirs are
        nodes["observer"].run(
            timeout=600,
            wait_for_live=True,
            alternate_chain_specs=chain_specs,
            time_control=tt.StartTimeControl(start_time=start_time + timedelta(seconds=joining - started)),
        )
        sampler.pids["observer"] = _pid_of(nodes["observer"])
        late_observer = {
            "witness_head_at_start": joined_at,
            "live_after_s": time.time() - joining,
            "witness_head_when_live": witness.get_last_block_number(),
            "observer_head_when_live": nodes["observer"].get_last_block_number(),
        }

    end_block = colony_start_block + spec.blocks
    witness.wait_for_block_with_number(end_block, timeout=(end_block - witness.get_last_block_number()) * 3 * 4 + 120)
    sampler.stop()
    last = witness.get_last_block_number()
    for node in nodes.values():
        node.close()
    return {
        **asdict(spec),
        "binaries": {role: spec.binary(role) for role in nodes},
        "roles": roles,
        "p2p_parameters": parameters,
        "p2p_loggers": loggers,
        "colony_start_block": colony_start_block,
        "seed_links_s": seed_links_s,
        "first": first,
        # the colonies start loading at colony_start_block: a witness already there when every node was live means
        # they started late, and the loaded window is short
        "late_start": first >= colony_start_block,
        "last": last,
        "late_observer": late_observer,
        "pids": pids,
        "tck": os.sysconf("SC_CLK_TCK"),
        "container_host": socket.gethostname(),
        "samples": sampler.samples,
        "links": sampler.links,
    }


def main(spec_path: Path) -> None:
    spec = RunSpec(**json.loads(spec_path.read_text()))
    result = run(spec)
    _write_json(Path(spec.out_dir) / "out.json", result)
