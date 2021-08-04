import math
import json
import re
from pathlib import Path
import shutil
import signal
import subprocess
import time

from test_tools import Account, constants, logger, paths_to_executables
from test_tools.exceptions import CommunicationError, NodeIsNotRunning, NodeProcessRunFailedError
from test_tools.node_api.node_apis import Apis
from test_tools.node_configs.default import create_default_config
from test_tools.private.block_log import BlockLog
from test_tools.private.snapshot import Snapshot
from test_tools.wallet import Wallet


class Node:
    __DEFAULT_WAIT_FOR_LIVE_TIMEOUT = 20

    class __Executable:
        def get_path(self):
            return paths_to_executables.get_path_of('hived')

        def get_build_version(self):
            if self.is_test_net_build():
                return 'testnet'
            elif self.is_main_net_build():
                return 'mainnet'
            else:
                return 'unrecognised'

        def is_test_net_build(self):
            error_message = self.__run_and_get_output('--chain-id')
            return error_message == 'Error parsing command line: the required argument for option \'--chain-id\' is missing'

        def is_main_net_build(self):
            error_message = self.__run_and_get_output('--chain-id')
            return error_message == 'Error parsing command line: unrecognised option \'--chain-id\''

        def __run_and_get_output(self, *arguments):
            result = subprocess.check_output(
                [str(self.get_path()), *arguments],
                stderr=subprocess.STDOUT,
            )

            return result.decode('utf-8').strip()

        def get_build_commit_hash(self):
            output = self.__run_and_get_output('--version')
            return json.loads(f'{{{output}}}')['version']['hive_git_revision']

    class __Process:
        def __init__(self, owner, directory, executable, logger):
            self.__owner = owner
            self.__process = None
            self.__directory = Path(directory).absolute()
            self.__executable = executable
            self.__logger = logger
            self.__files = {
                'stdout': None,
                'stderr': None,
            }

        def run(self, *, blocking, with_arguments=()):
            self.__directory.mkdir(exist_ok=True)
            self.__prepare_files_for_streams()

            command = [str(self.__executable.get_path()), '-d', '.', *with_arguments]
            self.__logger.debug(' '.join(item for item in command))

            run_subprocess = subprocess.run if blocking else subprocess.Popen
            process_handle = run_subprocess(
                command,
                cwd=self.__directory,
                **self.__files,
            )

            self.__process = process_handle if not blocking else None
            return process_handle.returncode if blocking else None

        def get_id(self):
            return self.__process.pid

        def __prepare_files_for_streams(self):
            for name in self.__files:
                if self.__files[name] is None:
                    self.__files[name] = open(self.__directory.joinpath(f'{name}.txt'), 'w')

        def workaround_stderr_parsing_problem(self):
            file = self.__files['stderr']
            if file is None:
                return

            file.close()
            self.__files['stderr'] = open(self.__directory.joinpath('stderr.txt'), 'w')

        def close(self):
            if self.__process is None:
                return

            self.__process.send_signal(signal.SIGINT)
            try:
                return_code = self.__process.wait(timeout=10)
                self.__logger.debug(f'Closed with {return_code} return code')
            except subprocess.TimeoutExpired:
                self.__process.kill()
                self.__process.wait()
                self.__logger.warning(f'Process was force-closed with SIGKILL, because didn\'t close before timeout')

        def close_opened_files(self):
            for name, file in self.__files.items():
                if file is not None:
                    file.close()
                    self.__files[name] = None

        def is_running(self):
            if not self.__process:
                return False

            return self.__process.poll() is None

        def get_stderr_file_path(self):
            stderr_file = self.__files['stderr']
            return Path(stderr_file.name) if stderr_file is not None else None

    def __init__(self, creator, name, directory):
        self.api = Apis(self)

        import weakref
        self.__creator = weakref.proxy(creator)
        self.__name = name
        self.directory = Path(directory).joinpath(self.__name).absolute()
        self.__produced_files = False
        self.__logger = logger.getLogger(f'{self.__creator}.{self.__name}')

        self.__executable = self.__Executable()
        self.__process = self.__Process(self, self.directory, self.__executable, self.__logger)
        self.__clean_up_policy = None

        self.config = create_default_config()

    def __str__(self):
        from test_tools.network import Network
        return f'{self.__creator}::{self.__name}' if isinstance(self.__creator, Network) else self.__name

    def __get_config_file_path(self):
        return self.directory / 'config.ini'

    def is_running(self):
        return self.__process.is_running()

    def is_able_to_produce_blocks(self):
        conditions = [
            self.config.enable_stale_production,
            self.config.required_participation == 0,
            bool(self.config.witness),
            bool(self.config.private_key),
        ]

        return all(conditions)

    def attach_wallet(self, timeout=15) -> Wallet:
        if not self.is_running():
            raise NodeIsNotRunning('Before attaching wallet you have to run node')

        return self.__creator.attach_wallet_to(self, timeout)

    def get_name(self):
        return self.__name

    def get_block_log(self, include_index=True):
        return BlockLog(self, self.directory.joinpath('blockchain/block_log'), include_index=include_index)

    def __is_p2p_plugin_started(self):
        return self.__any_line_in_stderr(lambda line: 'P2P Plugin started' in line)

    def __is_http_listening(self):
        return self.__any_line_in_stderr(lambda line: 'start listening for http requests' in line)

    def _is_ws_listening(self):
        return self.__any_line_in_stderr(lambda line: 'start listening for ws requests' in line)

    def __is_live(self):
        return self.__any_line_in_stderr(
            lambda line: 'transactions on block' in line or 'Generated block #' in line
        )

    def __is_snapshot_dumped(self):
        return self.__any_line_in_stderr(lambda line: 'Snapshot generation finished' in line)

    def __any_line_in_stderr(self, predicate):
        with open(self.__process.get_stderr_file_path()) as output:
            for line in output:
                if predicate(line):
                    return True

        return False

    def wait_number_of_blocks(self, blocks_to_wait, *, timeout=math.inf):
        assert blocks_to_wait > 0
        self.wait_for_block_with_number(self.get_last_block_number() + blocks_to_wait, timeout=timeout)

    def wait_for_block_with_number(self, number, *, timeout=math.inf):
        from test_tools.private.wait_for import wait_for
        wait_for(
            lambda: self.__is_block_with_number_reached(number),
            timeout=timeout,
            timeout_error_message=f'Waiting too long for block {number}',
            poll_time=2.0
        )

    def __is_block_with_number_reached(self, number):
        last = self.get_last_block_number()
        return last >= number

    def get_last_block_number(self):
        response = self.api.database.get_dynamic_global_properties()
        return response['result']['head_block_number']

    def _wait_for_p2p_plugin_start(self, timeout=10):
        from test_tools.private.wait_for import wait_for
        wait_for(self.__is_p2p_plugin_started, timeout=timeout,
                 timeout_error_message=f'Waiting too long for start of {self} p2p plugin')

    def _wait_for_live(self, timeout=__DEFAULT_WAIT_FOR_LIVE_TIMEOUT):
        from test_tools.private.wait_for import wait_for
        wait_for(self.__is_live, timeout=timeout,
                 timeout_error_message=f'Waiting too long for {self} live (to start produce or receive blocks)')

    def _send(self, method, params=None, jsonrpc='2.0', id=1):
        message = {
            'jsonrpc': jsonrpc,
            'id': id,
            'method': method,
        }

        if params is not None:
            message['params'] = params

        if self.config.webserver_http_endpoint is None:
            raise Exception('Webserver http endpoint is unknown')

        endpoint = f'http://{self._get_http_endpoint()}'

        self.__wait_for_http_listening()

        from test_tools import communication
        response = communication.request(endpoint, message)

        if 'error' in response:
            raise CommunicationError(f'Error detected in response from node {self}', message, response)

        return response

    def __wait_for_http_listening(self, timeout=10):
        from test_tools.private.wait_for import wait_for
        wait_for(self.__is_http_listening, timeout=timeout,
                 timeout_error_message=f'Waiting too long for {self} to start listening on http port')

    def get_id(self):
        response = self.api.network_node.get_info()
        return response['result']['node_id']

    def set_allowed_nodes(self, nodes):
        return self.api.network_node.set_allowed_peers(allowed_peers=[node.get_id() for node in nodes])

    def dump_config(self):
        assert not self.is_running()

        self.__logger.info('Config dumping started...')

        config_was_modified = self.config != create_default_config()
        self.__run_process(blocking=True, with_arguments=['--dump-config'], write_config_before_run=config_was_modified)

        self.config.load_from_file(self.__get_config_file_path())

        self.__logger.info('Config dumped')

    def dump_snapshot(self, *, close=False):
        self.close()

        self.__logger.info('Snapshot dumping started...')

        snapshot_path = Path('.')
        self.__ensure_that_plugin_required_for_snapshot_is_included()
        self.__run_process(
            blocking=close,
            with_arguments=[
                f'--dump-snapshot={snapshot_path}',
                *(['--exit-before-sync'] if close else []),
            ]
        )

        if not close:
            self.__wait_for_dumping_snapshot_finish()

        self.__logger.info('Snapshot dumped')

        return Snapshot(
            self.directory / 'snapshot' / snapshot_path,
            self.directory / 'blockchain/block_log',
            self.directory / 'blockchain/block_log.index',
            self,
        )

    def __ensure_that_plugin_required_for_snapshot_is_included(self):
        plugin_required_for_snapshots = 'state_snapshot'
        if plugin_required_for_snapshots not in self.config.plugin:
            self.config.plugin.append(plugin_required_for_snapshots)

    def __wait_for_dumping_snapshot_finish(self, timeout=15):
        from test_tools.private.wait_for import wait_for
        wait_for(self.__is_snapshot_dumped, timeout=timeout,
                 timeout_error_message=f'Waiting too long for {self} to dump snapshot')

    def __run_process(self, *, blocking, write_config_before_run=True, with_arguments=()):
        if write_config_before_run:
            self.config.write_to_file(self.__get_config_file_path())

        return_code = self.__process.run(blocking=blocking, with_arguments=with_arguments)

        if return_code is not None and return_code != 0:
            raise NodeProcessRunFailedError(f'Process closed with error code {return_code}', return_code)

        return return_code

    def run(
            self,
            *,
            load_snapshot_from=None,
            replay_from=None,
            stop_at_block=None,
            exit_before_synchronization=False,
            wait_for_live=None,
            timeout=__DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
    ):
        """
        :param wait_for_live: Stops execution until node will generate or receive blocks.
        :param timeout: If wait_for_live is set to True, this parameter sets how long waiting can take. When
                        timeout is reached, TimeoutError exception is thrown.
        """
        if not self.__executable.is_test_net_build():
            raise NotImplementedError(
                f'You have configured path to non-testnet hived build.\n'
                f'At the moment only testnet build is supported.\n'
                f'Your current hived path is: {paths_to_executables.get_path_of("hived")}\n'
                f'\n'
                f'Please check following page if you need help with paths configuration:\n'
                f'https://gitlab.syncad.com/hive/test-tools/-/blob/master/documentation/paths_to_executables.md'
            )

        if not self.__produced_files and self.directory.exists():
            shutil.rmtree(self.directory)
        self.directory.mkdir(parents=True, exist_ok=True)

        self.__set_unset_endpoints()

        # TODO: Remove below code when statsd support will be implemented.
        #       This is temporary workaround, for stderr parsing solution.
        #       When node is restarted, old stderr is parsed first, because
        #       new logs are at the bottom. Parser reads wrong informations.
        #       This workaround removes old logs.
        self.__process.workaround_stderr_parsing_problem()
        # ------------------------- End of workaround -------------------------

        log_message = f'Running {self}'
        additional_arguments = []
        if load_snapshot_from is not None:
            self.__handle_loading_snapshot(load_snapshot_from, additional_arguments)
            log_message += ', loading snapshot'

        if replay_from is not None:
            self.__handle_replay(replay_from, stop_at_block, additional_arguments)
            log_message += ', replaying'

        if exit_before_synchronization:
            if wait_for_live is not None:
                raise RuntimeError('wait_for_live can\'t be used with exit_before_synchronization')

            wait_for_live = False
            additional_arguments.append('--exit-before-sync')

            self.__logger.info(f'{log_message} and waiting for close...')
        elif wait_for_live is None:
            wait_for_live = True
            self.__logger.info(f'{log_message} and waiting for live...')
        else:
            self.__logger.info(f'{log_message} and NOT waiting for live...')

        return_code = self.__run_process(blocking=exit_before_synchronization, with_arguments=additional_arguments)

        self.__produced_files = True
        if wait_for_live:
            self._wait_for_live(timeout)

        self.__log_run_summary(return_code)

    def __handle_loading_snapshot(self, snapshot_source: Snapshot, additional_arguments: list):
        if not isinstance(snapshot_source, Snapshot):
            snapshot_source = Snapshot(
                snapshot_source,
                Path(snapshot_source).joinpath('../blockchain/block_log'),
                Path(snapshot_source).joinpath('../blockchain/block_log.index')
            )

        self.__ensure_that_plugin_required_for_snapshot_is_included()
        additional_arguments.append('--load-snapshot=.')
        snapshot_source.copy_to(self.directory)

    def __handle_replay(self, replay_source: BlockLog, stop_at_block: int, additional_arguments: list):
        if not isinstance(replay_source, BlockLog):
            replay_source = BlockLog(None, replay_source)

        additional_arguments.append('--force-replay')
        if stop_at_block is not None:
            additional_arguments.append(f'--stop-replay-at-block={stop_at_block}')

        blocklog_directory = self.directory.joinpath('blockchain')
        blocklog_directory.mkdir()
        replay_source.copy_to(blocklog_directory)

    def __log_run_summary(self, return_code):
        if self.is_running():
            message = f'Run with pid {self.__process.get_id()}, '

            endpoints = self.__get_opened_endpoints()
            if endpoints:
                message += f'with servers: {", ".join([f"{endpoint[1]}://{endpoint[0]}" for endpoint in endpoints])}'
            else:
                message += 'without any server'
        else:
            message = f'Run completed with return code {return_code}'

        message += f', {self.__executable.get_build_version()} build'
        message += f' commit={self.__executable.get_build_commit_hash()[:8]}'
        self.__logger.info(message)

    def __get_opened_endpoints(self):
        endpoints = [
            (self.config.webserver_http_endpoint, 'http'),
            (self.config.webserver_ws_endpoint, 'ws'),
            (self.config.webserver_unix_endpoint, 'unix'),
        ]

        return [endpoint for endpoint in endpoints if endpoint[0] is not None]

    def __set_unset_endpoints(self):
        if self.config.p2p_endpoint is None:
            self.config.p2p_endpoint = f'0.0.0.0:0'

        if self.config.webserver_http_endpoint is None:
            self.config.webserver_http_endpoint = f'0.0.0.0:0'

        if self.config.webserver_ws_endpoint is None:
            self.config.webserver_ws_endpoint = f'0.0.0.0:0'

    def _get_p2p_endpoint(self):
        self._wait_for_p2p_plugin_start()
        with open(self.__process.get_stderr_file_path()) as output:
            for line in output:
                if 'P2P node listening at ' in line:
                    endpoint = re.match(r'^.*P2P node listening at ([\d\.]+\:\d+)\s*$', line)[1]
                    return endpoint.replace('0.0.0.0', '127.0.0.1')

    def _get_http_endpoint(self):
        self.__wait_for_http_listening()
        with open(self.__process.get_stderr_file_path()) as output:
            for line in output:
                if 'start listening for http requests' in line:
                    endpoint = re.match(r'^.*start listening for http requests on ([\d\.]+\:\d+)\s*$', line)[1]
                    return endpoint.replace('0.0.0.0', '127.0.0.1')

    def _get_ws_endpoint(self):
        self.__wait_for_http_listening()
        with open(self.__process.get_stderr_file_path()) as output:
            for line in output:
                if 'start listening for ws requests' in line:
                    endpoint = re.match(r'^.*start listening for ws requests on ([\d\.]+\:\d+)\s*$', line)[1]
                    return endpoint.replace('0.0.0.0', '127.0.0.1')

    def close(self):
        self.__process.close()

    def handle_final_cleanup(self, *, default_policy: constants.NodeCleanUpPolicy):
        self.__process.close()
        self.__process.close_opened_files()
        self.__remove_files(default_policy=default_policy)

    def restart(self, wait_for_live=True, timeout=__DEFAULT_WAIT_FOR_LIVE_TIMEOUT):
        self.close()
        self.run(wait_for_live=wait_for_live, timeout=timeout)

    def __remove_files(self, *, default_policy: constants.NodeCleanUpPolicy):
        policy = default_policy if self.__clean_up_policy is None else self.__clean_up_policy

        if policy == constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES:
            pass
        elif policy == constants.NodeCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES:
            self.__remove_unneeded_files()
        elif policy == constants.NodeCleanUpPolicy.REMOVE_EVERYTHING:
            self.__remove_all_files()

    @staticmethod
    def __remove(path: Path):
        try:
            shutil.rmtree(path) if path.is_dir() else path.unlink()
        except FileNotFoundError:
            pass  # It is ok that file to remove was removed earlier or never existed

    def __remove_unneeded_files(self):
        unneeded_files_or_directories = [
            'blockchain/',
            'p2p/',
            'snapshot/',
        ]

        for unneeded in unneeded_files_or_directories:
            self.__remove(self.directory.joinpath(unneeded))

    def __remove_all_files(self):
        self.__remove(self.directory)

    def set_executable_file_path(self, executable_file_path):
        self.__executable_file_path = executable_file_path

    def set_clean_up_policy(self, policy: constants.NodeCleanUpPolicy):
        self.__clean_up_policy = policy
