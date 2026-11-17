"""
One harness run inside the test-tools environment: build the topology, replay, load it with colony, sample, record.

Runs in the container `harness.py` starts, with the run's tmpfs directory as working directory, so test-tools puts
every node's data under it. Writes roles.json as soon as the nodes exist and out.json when the run ends.
"""

from __future__ import annotations

import json
import os
import shutil
import socket
import threading
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from dataclasses import asdict, dataclass
from datetime import timedelta
from functools import partial
from pathlib import Path
from typing import TYPE_CHECKING, Any, TypeAlias

from beekeepy.exceptions import CommunicationError
from beekeepy.interfaces import P2PUrl

import test_tools as tt
from python.functional.p2p_load import p2p_loggers, p2p_parameters, procfs, replay_template, topology
from python.functional.p2p_load.spec import RunSpec
from test_tools import complex_networks as ttcn

if TYPE_CHECKING:
    from collections.abc import Callable

_SAMPLE_INTERVAL_S = 3
_LINKS_EVERY_N_SAMPLES = 10
_NODE_TIMEOUT_S = 300
# a node dials its seeds once at start and, refused, only after the p2p retry backoff (60-120 s); the driver redials
# them until every link is up, this long at most
_SEED_LINK_TIMEOUT_S = 60
_SEED_LINK_POLL_S = 1
# even redialed, a refused node dials again only on hived's next 10 s connect loop tick, so nodes start in waves
# this far apart (whole seconds: faketime's start time has no fraction), each one wave after its seeds
_START_WAVE_S = 1
# replayed-state templates kept (2.6 GiB of tmpfs each): the three plugin sets (witness, API node, colony) of an A/B
_KEEP_TEMPLATES = 6
# a two-host run: how long a host whose nodes are replayed waits for the other's, and the start signal
_GO_TIMEOUT_S = 1800
_GO_POLL_S = 0.2
_STOP_POLL_S = 1
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


@dataclass(frozen=True)
class _Endpoint:
    listen: P2PUrl  # what the node's p2p-endpoint binds
    address: str  # host:port its peers dial, as hived names its peers


def _endpoints(spec: RunSpec) -> dict[str, _Endpoint]:
    """Every listening node's p2p endpoint (the observer does not listen): on loopback, or on the hosts' network."""
    listening = [role for role in spec.roles() if role != "observer"]
    if spec.remote_host:
        return {
            role: _Endpoint(P2PUrl(f"0.0.0.0:{spec.endpoints[role].rpartition(':')[2]}"), spec.endpoints[role])
            for role in listening
        }
    free = iter(ttcn.generate_free_addresses(len(listening)))
    endpoints: dict[str, _Endpoint] = {}
    for role in listening:
        port = spec.colony_ports.get(role.removeprefix("colony")) if role.startswith("colony") else None
        # a netem-delayed colony listens on the port the host side filters on
        url = P2PUrl(f"127.0.0.1:{port}" if port else next(free))
        endpoints[role] = _Endpoint(url, url.as_string(with_protocol=False))
    return endpoints


def _wire(spec: RunSpec, nodes: dict[str, _Node]) -> tuple[topology.Wiring, dict[str, _Endpoint]]:
    """Configure the p2p endpoint, seeds and connection limit of every node of this host."""
    wiring = topology.wire(spec)
    endpoints = _endpoints(spec)
    for role, node in nodes.items():
        if role in endpoints:
            node.config.p2p_endpoint = endpoints[role].listen
        if wiring.seeds.get(role):
            node.config.p2p_seed_node = [P2PUrl(endpoints[peer].address) for peer in wiring.seeds[role]]
        if role in wiring.max_connections:
            node.config.p2p_max_connections = wiring.max_connections[role]
    return wiring, endpoints


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


def _wait_for_seed_links(
    links: list[tuple[str, str]], running: dict[str, _Node], endpoints: dict[str, _Endpoint]
) -> float:
    """
    Redial the seeds of the `running` nodes until each is linked to every seed of it; return how long that took.

    A link is up when either end lists the other's p2p endpoint as a peer: hived keeps one connection per pair of
    nodes, so the seed may hold the connection it dialed itself. A seed on the other host of a two-host run is only
    dialed; its own host checks its side. Raise RuntimeError naming the links still down after _SEED_LINK_TIMEOUT_S.
    """
    links = [(role, seed) for role, seed in links if role in running]
    started = time.time()
    while True:
        hosts = {role: _peer_hosts(node) for role, node in running.items()}
        down = [
            (role, seed)
            for role, seed in links
            if endpoints[seed].address not in hosts[role]
            and not (seed in running and endpoints.get(role) and endpoints[role].address in hosts[seed])
        ]
        if not down:
            return time.time() - started
        if time.time() - started > _SEED_LINK_TIMEOUT_S:
            names = ", ".join(f"{role} -> {seed}" for role, seed in down)
            raise RuntimeError(f"p2p links still down {_SEED_LINK_TIMEOUT_S} s after the nodes started: {names}")
        for role, seed in down:
            _add_node(running[role], endpoints[seed].address)
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
    """
    Every few seconds: a node's head, every node's per-thread CPU ticks and the host's busy ticks next to the
    driver's own (its whole process tree: every node, the colonies, the driver); now and then the p2p link map.

    The head is the witness's, or on the remote host of a two-host run that of one of its own nodes.
    """

    def __init__(self, head_node: _Node, pids: dict[str, int]) -> None:
        super().__init__(daemon=True)
        self._head_node = head_node
        self.pids = pids
        self.samples: list[dict[str, Any]] = []
        self.links: list[dict[str, Any]] = []
        self._stop_event = threading.Event()

    def run(self) -> None:
        while not self._stop_event.is_set():
            pids = dict(self.pids)
            try:
                head = self._head_node.get_last_block_number()
                # read back to back, so their difference is the CPU spent outside the run up to the same moment
                t, host_busy, own = time.time(), procfs.host_busy_ticks(), procfs.tree_ticks(os.getpid())
                ticks = {role: procfs.thread_ticks(pid) for role, pid in pids.items()}
                self.samples.append({"t": t, "head": head, "ticks": ticks, "host_busy": host_busy, "own": own})
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


def _replay(node: _Node, block_log: tt.BlockLog, chain_specs: tt.AlternateChainSpecs) -> None:
    node.run(
        replay_from=block_log,
        timeout=_NODE_TIMEOUT_S,
        alternate_chain_specs=chain_specs,
        exit_before_synchronization=True,
    )


def _initialize(node: _Node, chain_specs: tt.AlternateChainSpecs) -> None:
    """Have test-tools set up the node's directory and config: hived opens an empty state and exits at once."""
    node.run(timeout=_NODE_TIMEOUT_S, alternate_chain_specs=chain_specs, exit_before_synchronization=True)


def _in_parallel(calls: list[Callable[[], object]]) -> None:
    if not calls:
        return
    with ThreadPoolExecutor(max_workers=len(calls)) as pool:
        for future in [pool.submit(call) for call in calls]:
            future.result()


def _template_keys(spec: RunSpec, nodes: dict[str, _Node]) -> dict[str, str]:
    fixture = replay_template.fixture_digest(Path(spec.block_log))
    return {
        role: replay_template.template_key(
            Path(spec.binary(role)),
            fixture,
            node.config.plugin,
            node.config.shared_file_size,
            node.config.block_log_split,
        )
        for role, node in nodes.items()
    }


def _prepare_state(
    spec: RunSpec, nodes: dict[str, _Node], block_log: tt.BlockLog, chain_specs: tt.AlternateChainSpecs
) -> dict[str, Any]:
    """
    Give every node the state of the replayed block log, stopped before syncing; return what was replayed or reused.

    Without a template store every node replays, all in parallel. With one, a node whose template exists gets a copy
    of it; otherwise one node per missing template replays (in parallel with the others' setup) and its state
    becomes the template the others of its key get.
    """
    started = time.time()
    if not spec.replay_templates:
        _in_parallel([partial(_replay, node, block_log, chain_specs) for node in nodes.values()])
        replay_s = time.time() - started
        tt.logger.info(f"replayed every node in parallel in {replay_s:.1f} s")
        return {"templates": None, "replayed": list(nodes), "replay_s": replay_s, "copy_s": 0.0, "state_s": replay_s}

    store = replay_template.TemplateStore(Path(spec.replay_templates))
    keys = _template_keys(spec, nodes)
    templates = {key: store.find(key) for key in set(keys.values())}
    replayers = {key: next(r for r, k in keys.items() if k == key) for key, path in templates.items() if path is None}
    copied = [role for role in nodes if role not in replayers.values()]
    _in_parallel([
        *(partial(_replay, nodes[role], block_log, chain_specs) for role in replayers.values()),
        *(partial(_initialize, nodes[role], chain_specs) for role in copied),
    ])
    replay_s = time.time() - started
    for key, role in replayers.items():
        info = {"hived": spec.binary(role), "fixture": spec.block_log, "replayed_by": role, "replay_s": replay_s}
        store.save(key, nodes[role].directory / "blockchain", info)
        templates[key] = store.find(key)
    if replayers:
        removed = store.prune(_KEEP_TEMPLATES, protect=templates)
        if removed:
            tt.logger.info(f"removed the least recently used replay templates {removed}")

    def copy_template(role: str) -> None:
        template = templates[keys[role]]
        if template is None:
            raise RuntimeError(f"replay template {keys[role]} vanished before {role} got its copy")
        shutil.rmtree(nodes[role].directory / "blockchain")
        replay_template.copy_state(template, nodes[role].directory / "blockchain")

    copying = time.time()
    _in_parallel([partial(copy_template, role) for role in copied])
    copy_s = time.time() - copying
    tt.logger.info(
        f"state: replayed on {sorted(replayers.values()) or 'no node'} in {replay_s:.1f} s, "
        f"copied replay templates into {len(copied)} nodes in {copy_s:.1f} s"
    )
    return {
        "templates": {
            key: {"reused": key not in replayers, "roles": sorted(r for r, k in keys.items() if k == key)}
            for key in templates
        },
        "replayed": sorted(replayers.values()),
        "replay_s": replay_s,
        "copy_s": copy_s,
        "state_s": time.time() - started,
    }


def _wait_for_go(out_dir: Path, part: str) -> float:
    """
    Report this host's nodes replayed and wait for the shared start of a two-host run; return its moment.

    The host side writes `<part>.go` once both hosts are ready, holding a moment a few seconds ahead on this host's
    clock (corrected for the measured offset between the hosts), so both hosts start their nodes, and so their
    faked clocks, at the same real time.
    """
    _write_json(out_dir / f"{part}.ready", {"t": time.time()})
    go = out_dir / f"{part}.go"
    deadline = time.time() + _GO_TIMEOUT_S
    while not go.is_file():
        if time.time() > deadline:
            raise RuntimeError(f"no start signal {_GO_TIMEOUT_S} s after this host's nodes were ready")
        time.sleep(_GO_POLL_S)
    t0 = float(json.loads(go.read_text())["t0"])
    time.sleep(max(0.0, t0 - time.time()))
    return t0


def _wait_for_end(node: _Node, end_block: int, stop: Path) -> None:
    """The remote part's end: its node has the run's last block, or the local part, finished, wrote `stop`."""
    deadline = time.time() + (end_block - node.get_last_block_number()) * 3 * 4 + 120
    while node.get_last_block_number() < end_block and not stop.is_file():
        if time.time() > deadline:
            raise RuntimeError(f"{node} did not reach block {end_block}, and the local part did not finish")
        time.sleep(_STOP_POLL_S)


def run(spec: RunSpec, part: str = "local") -> dict[str, Any]:
    """Run the nodes of `part` ("local": the witness's host and the whole of a single-host run, or "remote")."""
    run_started = time.time()
    out_dir = Path(spec.out_dir)
    block_log = tt.BlockLog(Path(spec.block_log), "auto")
    chain_specs = tt.AlternateChainSpecs.parse_file(Path(spec.block_log) / tt.AlternateChainSpecs.FILENAME)
    colony_start_block = block_log.get_head_block_number() + spec.colony_start_delay

    nodes = {role: _create_node(spec, role, colony_start_block) for role in spec.roles_on(part)}
    wiring, endpoints = _wire(spec, nodes)
    parameters = _apply_p2p_parameters(spec, nodes)
    loggers = _enable_p2p_loggers(spec, nodes)
    roles = {role: node.directory.name for role, node in nodes.items()}
    _write_json(
        out_dir / "roles.json", {"roles": roles, "directories": {r: str(n.directory) for r, n in nodes.items()}}
    )

    created_s = time.time() - run_started
    state = _prepare_state(spec, nodes, block_log, chain_specs)
    for role, node in nodes.items():
        p2p_parameters.verify_config(node.config_file_path, parameters["applied"][role])
        p2p_loggers.verify_config(node.config_file_path, loggers[role])

    late = spec.late_observer_blocks > 0 and "observer" in nodes
    start_time = block_log.get_head_block_time()
    starting = {role: node for role, node in nodes.items() if not (late and role == "observer")}
    # both hosts of a two-host run wave the whole run's nodes alike, so their faked clocks agree
    waves = wiring.start_waves([r for r in spec.roles() if not (spec.late_observer_blocks and r == "observer")])
    started = _wait_for_go(out_dir, part) if spec.remote_host else time.time()
    with ThreadPoolExecutor(max_workers=len(starting)) as pool:
        futures = []
        for number, wave in enumerate(waves):
            # each wave's faked clock starts as much later as the wave does, so the clocks still agree
            offset = timedelta(seconds=number * _START_WAVE_S)
            time.sleep(max(0.0, started + offset.total_seconds() - time.time()))
            futures += [
                pool.submit(
                    starting[role].run,
                    timeout=_NODE_TIMEOUT_S,
                    wait_for_live=False,
                    alternate_chain_specs=chain_specs,
                    time_control=tt.StartTimeControl(start_time=start_time + offset),
                )
                for role in wave
                if role in starting
            ]
        for future in futures:
            future.result()
    # a seed slow to listen still costs its dialer the connect loop tick; the colonies must not wait out the backoff
    seed_links_s = _wait_for_seed_links(wiring.links, starting, endpoints)
    for node in starting.values():
        node.wait_for_live_mode(timeout=_NODE_TIMEOUT_S)
    live_s = time.time() - started
    tt.logger.info(f"every node live {live_s:.1f} s after the start, seed links up after {seed_links_s:.1f} s")
    head_node = nodes["witness"] if part == "local" else next(iter(starting.values()))
    pids = {role: _pid_of(node) for role, node in starting.items()}
    first = head_node.get_last_block_number()
    sampler = _Sampler(head_node, pids)
    sampler.start()

    late_observer: dict[str, Any] | None = None
    if late:
        witness = nodes["witness"]
        join_block = colony_start_block + spec.late_observer_blocks
        witness.wait_for_block_with_number(join_block, timeout=(join_block - first) * 3 * 4 + 120)
        joined_at = witness.get_last_block_number()
        joining = time.time()
        # the others' faked clocks read start_time at `started`; the observer's starts where theirs are
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
    if part == "local":
        head_node.wait_for_block_with_number(
            end_block, timeout=(end_block - head_node.get_last_block_number()) * 3 * 4 + 120
        )
    else:
        _wait_for_end(head_node, end_block, out_dir / f"{part}.stop")
    sampler.stop()
    last = head_node.get_last_block_number()
    closing = time.time()
    _in_parallel([node.close for node in nodes.values()])
    timings = {
        "create_s": created_s,
        "state_s": state["state_s"],
        "start_to_live_s": live_s,
        "live_to_end_s": closing - started - live_s,
        "close_s": time.time() - closing,
        "total_s": time.time() - run_started,
    }
    tt.logger.info(f"phase timings: {json.dumps(timings)}")
    return {
        **asdict(spec),
        "part": part,
        "binaries": {role: spec.binary(role) for role in spec.roles()},
        "roles": roles,
        "p2p_parameters": parameters,
        "p2p_loggers": loggers,
        "colony_start_block": colony_start_block,
        "seed_links_s": seed_links_s,
        "replay": state,
        "timings": timings,
        "first": first,
        # the colonies start loading at colony_start_block: a witness already there when every node was live means
        # they started late, and the loaded window is short
        "late_start": first >= colony_start_block,
        "last": last,
        "late_observer": late_observer,
        "pids": pids,
        "tck": os.sysconf("SC_CLK_TCK"),
        "nproc": procfs.cpu_count(),
        "container_host": socket.gethostname(),
        "samples": sampler.samples,
        "links": sampler.links,
    }


def main(spec_path: Path, part: str = "local") -> None:
    spec = RunSpec(**json.loads(spec_path.read_text()))
    result = run(spec, part)
    _write_json(Path(spec.out_dir) / "out.json", result)
