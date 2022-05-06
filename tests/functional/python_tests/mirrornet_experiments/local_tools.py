from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from typing import Iterable, Optional, TYPE_CHECKING

from test_tools import Account, Asset, BlockLog, logger, Wallet

if TYPE_CHECKING:
    from test_tools.private.node import Node
    from test_tools import World


class Mirrornet:
    SHARED_FILE_SIZE = '1G'  # FIXME: What should be the default value?

    def __init__(self, world: World, block_log_path: str, block_log_head_block_time: str, chain_id: int = 42,
                 skeleton_key: str = '5JhCDhQK2GaShbTnWWHXH2k5jtZi4TqotihFzWo4doJvbxnmVnc'):
        self.world = world
        self.chain_id = chain_id
        self.skeleton_key = skeleton_key
        self.block_log_path = block_log_path
        self.block_log_head_block_time = block_log_head_block_time
        self.init_node: Optional[Node] = None
        self.__time_offset: Optional[str] = None

    @property
    def time_offset(self) -> str:
        if self.__time_offset is None:
            t0 = datetime.strptime(self.block_log_head_block_time, '@%Y-%m-%d %H:%M:%S').replace(tzinfo=timezone.utc)
            diff = datetime.now(timezone.utc) - t0
            self.__time_offset = f'-{int(diff.total_seconds())}'

        return self.__time_offset

    def create_and_run_init_node(self, witnesses: Iterable[str]) -> Node:
        self.init_node = self.world.create_init_node()
        self.__preconfigure_node(self.init_node)

        self.init_node.config.witness = witnesses
        self.init_node.config.private_key = self.skeleton_key

        logger.info(f'Running {self.init_node}...')
        self.init_node.run(
            replay_from=self.block_log_path,
            arguments=[f'--chain-id={self.chain_id}', f'--skeleton-key={self.skeleton_key}'],
            time_offset=self.time_offset,
        )

        logger.info(f'{self.init_node} is replayed and produce blocks')

        return self.init_node

    def create_witness_node(self, witnesses: Iterable[str]) -> Node:
        witness_node = self.world.create_witness_node(witnesses=witnesses)
        self.__preconfigure_node(witness_node)
        return witness_node

    @classmethod
    def __preconfigure_node(cls, node: Node) -> None:
        node.config.shared_file_size = cls.SHARED_FILE_SIZE

        # By default, node has configured mainnet p2p seed nodes, which makes no sense.
        # Here these seed nodes are cleared with dirty trick (non-existing address works as no p2p seed nodes).
        node.config.p2p_seed_node = '0.0.0.0:12345'

        # [TEMPORARY] To speed up replay
        node.config.plugin.remove('account_history_rocksdb')
        node.config.plugin.remove('account_history_api')

    def run_witness_nodes(self, *nodes: Node) -> None:
        self.__replay_and_stop(*nodes)
        self.__run_prepared_nodes(*nodes)

        all_witnesses = [witness for node in nodes for witness in node.config.witness]
        self.__deactivate_witnesses_on_init_node_and_activate_them_on_witness_nodes(all_witnesses)

    def __replay_and_stop(self, *nodes: Node) -> None:
        executor = ThreadPoolExecutor(max_workers=8)
        futures = []
        for node in nodes:
            futures.append(
                executor.submit(
                    node.run,
                    replay_from=BlockLog(None, self.block_log_path, include_index=False),
                    arguments=[f'--chain-id={self.chain_id}', f'--skeleton-key={self.skeleton_key}'],
                    exit_before_synchronization=True,
                )
            )

        for future in futures:
            future.result()

        logger.info('All witness nodes replayed.')

    def __run_prepared_nodes(self, node: Node, *nodes: Node):
        node.config.p2p_seed_node = self.init_node.get_p2p_endpoint()
        node.run(
            arguments=[f'--chain-id={self.chain_id}', f'--skeleton-key={self.skeleton_key}'],
            time_offset=self.time_offset,
        )
        logger.info(f'{node} run.')

        for node_ in nodes:
            node_.config.p2p_seed_node = node.get_p2p_endpoint()
            node_.run(
                arguments=[f'--chain-id={self.chain_id}', f'--skeleton-key={self.skeleton_key}'],
                time_offset=self.time_offset,
            )
            logger.info(f'{node_} run.')

    def __deactivate_witnesses_on_init_node_and_activate_them_on_witness_nodes(
            self, witnesses: Iterable[str], wallet: Optional[Wallet] = None) -> None:
        logger.info('Deactivate witnesses on init node and activate them on witness nodes...')

        if wallet is None:
            wallet = Wallet(attach_to=self.init_node, additional_arguments=[f'--chain-id={self.chain_id}'])

        wallet.api.import_key(self.skeleton_key)

        for witness in witnesses:
            wallet.api.update_witness(
                witness, 'url', Account(witness).public_key,
                {'account_creation_fee': Asset.Hive(3), 'maximum_block_size': 65536, 'sbd_interest_rate': 0}
            )

        wallet.close()
