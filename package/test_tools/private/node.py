import json
import math
import os
from pathlib import Path
import shutil
import signal
import subprocess
from threading import Event
import time
from typing import List, Optional, Union
import warnings
import weakref

from test_tools import communication, constants, exceptions, network, paths_to_executables
from test_tools.node_api.node_apis import Apis
from test_tools.node_configs.default import create_default_config
from test_tools.private.block_log import BlockLog
from test_tools.private.raise_exception_helper import RaiseExceptionHelper
from test_tools.private.logger.logger_internal_interface import logger
from test_tools.private.node_http_server import NodeHttpServer
from test_tools.private.node_message import NodeMessage
from test_tools.private.snapshot import Snapshot
from test_tools.private.url import Url
from test_tools.private.wait_for import wait_for, wait_for_event


class Node:
    # pylint: disable=too-many-instance-attributes, too-many-public-methods
    # This pylint warning is right, but this refactor has low priority. Will be done later...

    __DEFAULT_WAIT_FOR_LIVE_TIMEOUT = 20

    class __Executable:
        def __init__(self):
            self.__path = None

        def get_path(self):
            return paths_to_executables.get_path_of('hived') if self.__path is None else self.__path

        def set_path(self, path):
            self.__path = path

        def get_build_version(self):
            if self.is_test_net_build():
                return 'testnet'

            if self.is_main_net_build():
                return 'mainnet'

            return 'unrecognised'

        def is_test_net_build(self):
            error_message = self.__run_and_get_output('--chain-id')
            return error_message == 'Error parsing command line: '\
                                    'the required argument for option \'--chain-id\' is missing'

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

        def get_supported_plugins(self) -> List[str]:
            output = self.__run_and_get_output('--list-plugins')
            return output.split('\n')

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

        def run(self, *, blocking, with_arguments=(), with_time_offset=None, extra_env_variables=None):
            self.__directory.mkdir(exist_ok=True)
            self.__prepare_files_for_streams()

            command = [str(self.__executable.get_path()), '-d', '.', *with_arguments]
            self.__logger.debug(' '.join(item for item in command))

            env = dict(os.environ)
            # Concat the variables provided with the system ones
            # note that the env vars provided as param superseeds those provided by the system
            if extra_env_variables is not None:
                env.update(extra_env_variables)
            if with_time_offset is not None:
                self.__configure_fake_time(env, with_time_offset)

            if blocking:
                subprocess.run(
                    command,
                    cwd=self.__directory,
                    **self.__files,
                    env=env,
                    check=True,
                )
            else:
                # pylint: disable=consider-using-with
                # Process created here have to exist longer than current scope
                self.__process = subprocess.Popen(
                    command,
                    cwd=self.__directory,
                    env=env,
                    **self.__files,
                )

        def get_id(self):
            return self.__process.pid

        def __configure_fake_time(self, env, time_offset):
            libfaketime_path = os.getenv('LIBFAKETIME_PATH') or '/usr/lib/x86_64-linux-gnu/faketime/libfaketime.so.1'
            if not Path(libfaketime_path).is_file():
                raise RuntimeError(f'libfaketime was not found at {libfaketime_path}')
            self.__logger.info(f"using time_offset {time_offset}")
            env['LD_PRELOAD'] = libfaketime_path
            env['FAKETIME'] = time_offset
            env['FAKETIME_DONT_RESET'] = '1'
            env['TZ'] = 'UTC'

        def __prepare_files_for_streams(self):
            for name, file in self.__files.items():
                if file is None:
                    # pylint: disable=consider-using-with
                    # Files opened here have to exist longer than current scope
                    self.__files[name] = open(self.__directory.joinpath(f'{name}.txt'), 'w', encoding='utf-8')

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
                warnings.warn('Process was force-closed with SIGKILL, because didn\'t close before timeout')

            self.__process = None

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

    class __NotificationsServer:
        def __init__(self, node: 'Node', logger):
            self.node: 'Node' = node
            self.server = NodeHttpServer(self)
            self.__logger = logger

            self.http_listening_event = Event()
            self.http_endpoint: Optional[str] = None

            self.ws_listening_event = Event()
            self.ws_endpoint: Optional[str] = None

            self.p2p_plugin_started_event = Event()
            self.p2p_endpoint: Optional[str] = None

            self.synchronization_started_event = Event()
            self.live_mode_entered_event = Event()

            self.replay_finished_event = Event()

            self.snapshot_dumped_event = Event()

        def listen(self):
            self.node.config.notifications_endpoint = f'127.0.0.1:{self.server.port}'
            self.server.run()

            self.__logger.debug(f'Notifications server is listening on {self.node.config.notifications_endpoint}...')

        def notify(self, message):
            if message['name'] == 'webserver listening':
                details = message['value']
                if details['type'] == 'HTTP':
                    endpoint = f'{details["address"].replace("0.0.0.0", "127.0.0.1")}:{details["port"]}'
                    self.http_endpoint = Url(endpoint, protocol='http').as_string(with_protocol=False)
                    self.http_listening_event.set()
                elif details['type'] == 'WS':
                    endpoint = f'{details["address"].replace("0.0.0.0", "127.0.0.1")}:{details["port"]}'
                    self.ws_endpoint = Url(endpoint, protocol='ws').as_string(with_protocol=False)
                    self.ws_listening_event.set()
            elif message['name'] == 'hived_status':
                details = message['value']
                if details['current_status'] == 'finished replaying':
                    self.replay_finished_event.set()
                elif details['current_status'] == 'finished dumping snapshot':
                    self.snapshot_dumped_event.set()
                elif details['current_status'] == 'syncing':
                    self.synchronization_started_event.set()
                elif details['current_status'] == 'entering live mode':
                    self.live_mode_entered_event.set()
            elif message['name'] == 'P2P listening':
                details = message['value']
                endpoint = f'{details["address"].replace("0.0.0.0", "127.0.0.1")}:{details["port"]}'
                self.p2p_endpoint = Url(endpoint).as_string(with_protocol=False)
                self.p2p_plugin_started_event.set()
            elif message['name'] == 'error':
                RaiseExceptionHelper.raise_exception_in_main_thread(
                    exceptions.InternalNodeError(f'{self.node}: {message["value"]["message"]}')
                )

            self.__logger.info(f'Received message: {message}')

        def close(self):
            self.server.close()

            self.http_listening_event.clear()
            self.ws_listening_event.clear()
            self.p2p_plugin_started_event.clear()
            self.synchronization_started_event.clear()
            self.live_mode_entered_event.clear()
            self.replay_finished_event.clear()
            self.snapshot_dumped_event.clear()

            self.__logger.debug('Notifications server closed')

    def __init__(self, creator, name, directory):
        self.api = Apis(self)

        self.__creator = weakref.proxy(creator)
        self.__name = name
        self.directory = Path(directory).joinpath(self.__name).absolute()
        self.__produced_files = False
        self.__logger = logger.create_child_logger(f'{self.__creator}.{self.__name}')

        self.__executable = self.__Executable()
        self.__process = self.__Process(self, self.directory, self.__executable, self.__logger)
        self.__notifications = self.__NotificationsServer(self, self.__logger)
        self.__clean_up_policy = None

        self.config = create_default_config()

    def __str__(self):
        return f'{self.__creator}.{self.__name}' if isinstance(self.__creator, network.Network) else self.__name

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

    def get_name(self):
        return self.__name

    def get_block_log(self, include_index=True):
        return BlockLog(self, self.directory.joinpath('blockchain/block_log'), include_index=include_index)

    def get_supported_plugins(self) -> List[str]:
        return self.__executable.get_supported_plugins()

    def wait_number_of_blocks(self, blocks_to_wait, *, timeout=math.inf):
        assert blocks_to_wait > 0
        self.wait_for_block_with_number(self.get_last_block_number() + blocks_to_wait, timeout=timeout)

    def wait_for_block_with_number(self, number, *, timeout=math.inf):
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
        return response['head_block_number']

    def __wait_for_p2p_plugin_start(self, timeout=10):
        if not self.__notifications.p2p_plugin_started_event.wait(timeout=timeout):
            raise TimeoutError(f'Waiting too long for start of {self} p2p plugin')

    def send(self, method, params=None, jsonrpc='2.0', id_=1, *, only_result: bool = True):
        if self.config.webserver_http_endpoint is None:
            raise Exception('Webserver http endpoint is unknown')

        endpoint = f'http://{self.get_http_endpoint()}'

        self.__wait_for_http_listening()

        message = NodeMessage(method, params, jsonrpc, id_).as_json()
        response = communication.request(endpoint, message)

        return response['result'] if only_result else response

    def __wait_for_http_listening(self, timeout=10):
        if not self.__notifications.http_listening_event.wait(timeout):
            raise TimeoutError(f'Waiting too long for {self} to start listening on http port')

    def __wait_for_ws_listening(self, timeout=10):
        if not self.__notifications.ws_listening_event.wait(timeout):
            raise TimeoutError(f'Waiting too long for {self} to start listening on ws port')

    def get_id(self):
        response = self.api.network_node.get_info()
        return response['node_id']

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
            self.__notifications.snapshot_dumped_event.wait()

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

    def __run_process(self,
                      *,
                      blocking,
                      write_config_before_run=True,
                      with_arguments=(),
                      with_time_offset=None,
                      env=None
    ):
        self.__notifications.listen()

        if write_config_before_run:
            self.config.write_to_file(self.__get_config_file_path())

        self.__process.run(
            blocking=blocking,
            with_arguments=with_arguments,
            with_time_offset=with_time_offset,
            extra_env_variables=env
        )

        if blocking:
            self.__notifications.close()

    def run(
            self,
            *,
            load_snapshot_from=None,
            replay_from=None,
            stop_at_block=None,
            exit_before_synchronization: bool = False,
            wait_for_live=None,
            timeout=__DEFAULT_WAIT_FOR_LIVE_TIMEOUT,
            time_offset=None,
            env=None,
    ):
        """
        :param wait_for_live: Stops execution until node will generate or receive blocks.
        :param timeout: If wait_for_live is set to True, this parameter sets how long waiting can take. When
                        timeout is reached, TimeoutError exception is thrown.
        """
        assert timeout >= 0
        deadline = time.time() + timeout

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

        log_message = f'Running {self}'
        if time_offset is not None:
            log_message += f' with time offset {time_offset}'

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

        self.__run_process(
            blocking=exit_before_synchronization,
            with_arguments=additional_arguments,
            with_time_offset=time_offset,
            env=env
        )

        if replay_from is not None and not exit_before_synchronization:
            self.__notifications.replay_finished_event.wait()

        self.__produced_files = True

        if not exit_before_synchronization:
            wait_for_event(self.__notifications.synchronization_started_event, deadline=deadline,
                           exception_message='Synchronization not started on time.')

            wait_for_event(self.__notifications.http_listening_event, deadline=deadline,
                           exception_message='HTTP server didn\'t start listening on time.')

            wait_for_event(self.__notifications.ws_listening_event, deadline=deadline,
                           exception_message='WS server didn\'t start listening on time.')

            if wait_for_live:
                wait_for_event(self.__notifications.live_mode_entered_event, deadline=deadline,
                               exception_message='Live mode not activated on time.')

        self.__log_run_summary()

    def __handle_loading_snapshot(self, snapshot_source: Union[str, Path, Snapshot], additional_arguments: list):
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

    def __log_run_summary(self):
        if self.is_running():
            message = f'Run with pid {self.__process.get_id()}, '

            endpoints = self.__get_opened_endpoints()
            if endpoints:
                message += f'with servers: {", ".join([f"{endpoint[1]}://{endpoint[0]}" for endpoint in endpoints])}'
            else:
                message += 'without any server'
        else:
            message = 'Run completed'

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
            self.config.p2p_endpoint = '0.0.0.0:0'

        if self.config.webserver_http_endpoint is None:
            self.config.webserver_http_endpoint = '0.0.0.0:0'

        if self.config.webserver_ws_endpoint is None:
            self.config.webserver_ws_endpoint = '0.0.0.0:0'

    def get_p2p_endpoint(self):
        self.__wait_for_p2p_plugin_start()
        return self.__notifications.p2p_endpoint

    def get_http_endpoint(self):
        self.__wait_for_http_listening()
        return self.__notifications.http_endpoint

    def get_ws_endpoint(self):
        self.__wait_for_ws_listening()
        return self.__notifications.ws_endpoint

    def close(self):
        self.__process.close()
        self.__notifications.close()

    def handle_final_cleanup(self, *, default_policy: constants.NodeCleanUpPolicy):
        self.close()
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
            'snapshot/',
        ]

        for unneeded in unneeded_files_or_directories:
            self.__remove(self.directory.joinpath(unneeded))

    def __remove_all_files(self):
        self.__remove(self.directory)

    def set_executable_file_path(self, executable_file_path):
        self.__executable.set_path(executable_file_path)

    def set_clean_up_policy(self, policy: constants.NodeCleanUpPolicy):
        self.__clean_up_policy = policy
