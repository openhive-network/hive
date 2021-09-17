import re
import shutil
import signal
import subprocess
from typing import Optional, TYPE_CHECKING
import warnings

from test_tools import communication, paths_to_executables
from test_tools.account import Account
from test_tools.exceptions import CommunicationError
from test_tools.private.logger.logger_internal_interface import logger
from test_tools.private.scope import context, ScopedObject
from test_tools.private.wait_for import wait_for

if TYPE_CHECKING:
    from test_tools.private.node import Node


class Wallet(ScopedObject):
    # pylint: disable=too-many-instance-attributes
    # This pylint warning is right, but this refactor has low priority. Will be done later...

    class __Api:
        # pylint: disable=invalid-name, too-many-arguments, too-many-public-methods
        # Wallet api is out of TestTools control

        class __TransactionBuilder:
            '''Helper class for sending multiple operations in single transaction'''

            def __init__(self):
                self.__transaction = None

            def append_operation(self, response):
                if self.__transaction is None:
                    self.__transaction = response['result']
                else:
                    operation = response['result']['operations'][0]
                    self.__transaction['operations'].append(operation)

            def get_transaction(self):
                return self.__transaction

        def __init__(self, wallet):
            self.__wallet = wallet
            self.__transaction_builder = None

        def _start_gathering_operations_for_single_transaction(self):
            if self.__transaction_builder is not None:
                raise RuntimeError('You cannot create transaction inside another transaction')

            self.__transaction_builder = self.__TransactionBuilder()

        def _send_gathered_operations_as_single_transaction(self, *, broadcast):
            transaction = self.__transaction_builder.get_transaction()
            self.__transaction_builder = None

            return self.sign_transaction(transaction, broadcast=broadcast) if transaction is not None else None

        def __send(self, method_, jsonrpc='2.0', id_=0, **params):
            if 'broadcast' in params:
                self.__handle_broadcast_parameter(params)

            response = self.__wallet.send(method_, *list(params.values()), jsonrpc=jsonrpc, id_=id_)

            if self.__is_transaction_build_in_progress():
                self.__transaction_builder.append_operation(response)

            return response

        def __handle_broadcast_parameter(self, params):
            if params['broadcast'] is None:
                params['broadcast'] = self.__get_default_broadcast_value()
            elif self.__is_transaction_build_in_progress():
                if params['broadcast'] is True:
                    raise RuntimeError(
                        'You cannot broadcast api call during transaction building.\n'
                        '\n'
                        'Replace broadcast parameter with value False or better -- remove it\n'
                        'completely, because it is default value during transaction building.'
                    )

                warnings.warn(
                    'Avoid explicit setting "broadcast" parameter to False during registering operations in\n'
                    'transaction. False is a default value in this context. It is considered bad practice,\n'
                    'because obscures code and decreases its readability.'
                )
            elif params['broadcast'] is True and not self.__is_transaction_build_in_progress():
                warnings.warn(
                    'Avoid explicit setting "broadcast" parameter to True in this context, it is default value.\n'
                    'It is considered bad practice, because obscures code and decreases its readability.'
                )

        def __get_default_broadcast_value(self):
            return not self.__is_transaction_build_in_progress()

        def __is_transaction_build_in_progress(self):
            return self.__transaction_builder is not None

        def about(self):
            return self.__send('about')

        def cancel_order(self, owner, orderid, broadcast=None):
            return self.__send('cancel_order', owner=owner, orderid=orderid, broadcast=broadcast)

        def cancel_transfer_from_savings(self, from_, request_id, broadcast=None):
            return self.__send('cancel_transfer_from_savings', from_=from_, request_id=request_id, broadcast=broadcast)

        def change_recovery_account(self, owner, new_recovery_account, broadcast=None):
            return self.__send('change_recovery_account', owner=owner, new_recovery_account=new_recovery_account,
                               broadcast=broadcast)

        def claim_account_creation(self, creator, fee, broadcast=None):
            return self.__send('claim_account_creation', creator=creator, fee=fee, broadcast=broadcast)

        def claim_account_creation_nonblocking(self, creator, fee, broadcast=None):
            return self.__send('claim_account_creation_nonblocking', creator=creator, fee=fee, broadcast=broadcast)

        def claim_reward_balance(self, account, reward_hive, reward_hbd, reward_vests, broadcast=None):
            return self.__send('claim_reward_balance', account=account, reward_hive=reward_hive, reward_hbd=reward_hbd,
                               reward_vests=reward_vests, broadcast=broadcast)

        def convert_hbd(self, from_, amount, broadcast=None):
            return self.__send('convert_hbd', from_=from_, amount=amount, broadcast=broadcast)

        def convert_hive_with_collateral(self, from_, collateral_amount, broadcast=None):
            return self.__send('convert_hive_with_collateral', from_=from_, collateral_amount=collateral_amount,
                               broadcast=broadcast)

        def create_account(self, creator, new_account_name, json_meta, broadcast=None):
            return self.__send('create_account', creator=creator, new_account_name=new_account_name,
                               json_meta=json_meta, broadcast=broadcast)

        def create_account_delegated(self, creator, hive_fee, delegated_vests, new_account_name, json_meta,
                                     broadcast=None):
            return self.__send('create_account_delegated', creator=creator, hive_fee=hive_fee,
                               delegated_vests=delegated_vests, new_account_name=new_account_name, json_meta=json_meta,
                               broadcast=broadcast)

        def create_account_with_keys(self, creator, newname, json_meta, owner, active, posting, memo, broadcast=None):
            return self.__send('create_account_with_keys', creator=creator, newname=newname, json_meta=json_meta,
                               owner=owner, active=active, posting=posting, memo=memo, broadcast=broadcast)

        def create_account_with_keys_delegated(self, creator, hive_fee, delegated_vests, newname, json_meta, owner,
                                               active, posting, memo, broadcast=None):
            return self.__send('create_account_with_keys_delegated', creator=creator, hive_fee=hive_fee,
                               delegated_vests=delegated_vests, newname=newname, json_meta=json_meta, owner=owner,
                               active=active, posting=posting, memo=memo, broadcast=broadcast)

        def create_funded_account_with_keys(self, creator, new_account_name, initial_amount, memo, json_meta, owner_key,
                                            active_key, posting_key, memo_key, broadcast=None):
            return self.__send('create_funded_account_with_keys', creator=creator, new_account_name=new_account_name,
                               initial_amount=initial_amount, memo=memo, json_meta=json_meta, owner_key=owner_key,
                               active_key=active_key, posting_key=posting_key, memo_key=memo_key, broadcast=broadcast)

        def create_order(self, owner, order_id, amount_to_sell, min_to_receive, fill_or_kill, expiration,
                         broadcast=None):
            return self.__send('create_order', owner=owner, order_id=order_id, amount_to_sell=amount_to_sell,
                               min_to_receive=min_to_receive, fill_or_kill=fill_or_kill, expiration=expiration,
                               broadcast=broadcast)

        def create_proposal(self, creator, receiver, start_date, end_date, daily_pay, subject, permlink,
                            broadcast=None):
            return self.__send('create_proposal', creator=creator, receiver=receiver, start_date=start_date,
                               end_date=end_date, daily_pay=daily_pay, subject=subject, permlink=permlink,
                               broadcast=broadcast)

        def decline_voting_rights(self, account, decline, broadcast=None):
            return self.__send('decline_voting_rights', account=account, decline=decline, broadcast=broadcast)

        def decrypt_memo(self, memo):
            return self.__send('decrypt_memo', memo=memo)

        def delegate_vesting_shares(self, delegator, delegatee, vesting_shares, broadcast=None):
            return self.__send('delegate_vesting_shares', delegator=delegator, delegatee=delegatee,
                               vesting_shares=vesting_shares, broadcast=broadcast)

        def delegate_vesting_shares_and_transfer(self, delegator, delegatee, vesting_shares, transfer_amount,
                                                 transfer_memo, broadcast=None):
            return self.__send('delegate_vesting_shares_and_transfer', delegator=delegator, delegatee=delegatee,
                               vesting_shares=vesting_shares, transfer_amount=transfer_amount,
                               transfer_memo=transfer_memo, broadcast=broadcast)

        def delegate_vesting_shares_and_transfer_nonblocking(self, delegator, delegatee, vesting_shares,
                                                             transfer_amount, transfer_memo, broadcast=None):
            return self.__send('delegate_vesting_shares_and_transfer_nonblocking', delegator=delegator,
                               delegatee=delegatee, vesting_shares=vesting_shares, transfer_amount=transfer_amount,
                               transfer_memo=transfer_memo, broadcast=broadcast)

        def delegate_vesting_shares_nonblocking(self, delegator, delegatee, vesting_shares, broadcast=None):
            return self.__send('delegate_vesting_shares_nonblocking', delegator=delegator, delegatee=delegatee,
                               vesting_shares=vesting_shares, broadcast=broadcast)

        def escrow_approve(self, from_, to, agent, who, escrow_id, approve, broadcast=None):
            return self.__send('escrow_approve', from_=from_, to=to, agent=agent, who=who, escrow_id=escrow_id,
                               approve=approve, broadcast=broadcast)

        def escrow_dispute(self, from_, to, agent, who, escrow_id, broadcast=None):
            return self.__send('escrow_dispute', from_=from_, to=to, agent=agent, who=who, escrow_id=escrow_id,
                               broadcast=broadcast)

        def escrow_release(self, from_, to, agent, who, receiver, escrow_id, hbd_amount, hive_amount, broadcast=None):
            return self.__send('escrow_release', from_=from_, to=to, agent=agent, who=who, receiver=receiver,
                               escrow_id=escrow_id, hbd_amount=hbd_amount, hive_amount=hive_amount, broadcast=broadcast)

        def escrow_transfer(self, from_, to, agent, escrow_id, hbd_amount, hive_amount, fee, ratification_deadline,
                            escrow_expiration, json_meta, broadcast=None):
            return self.__send('escrow_transfer', from_=from_, to=to, agent=agent, escrow_id=escrow_id,
                               hbd_amount=hbd_amount, hive_amount=hive_amount, fee=fee,
                               ratification_deadline=ratification_deadline, escrow_expiration=escrow_expiration,
                               json_meta=json_meta, broadcast=broadcast)

        def estimate_hive_collateral(self, hbd_amount_to_get):
            return self.__send('estimate_hive_collateral', hbd_amount_to_get=hbd_amount_to_get)

        def find_proposals(self, proposal_ids):
            return self.__send('find_proposals', proposal_ids=proposal_ids)

        def find_recurrent_transfers(self, from_):
            return self.__send('find_recurrent_transfers', from_=from_)

        def follow(self, follower, following, what, broadcast=None):
            return self.__send('follow', follower=follower, following=following, what=what, broadcast=broadcast)

        def get_account(self, account_name):
            return self.__send('get_account', account_name=account_name)

        def get_accounts(self, account_names):
            return self.__send('get_accounts', account_names=account_names)

        def get_account_history(self, account, from_, limit):
            return self.__send('get_account_history', account=account, from_=from_, limit=limit)

        def get_active_witnesses(self):
            return self.__send('get_active_witnesses')

        def get_block(self, num):
            return self.__send('get_block', num=num)

        def get_collateralized_conversion_requests(self, owner):
            return self.__send('get_collateralized_conversion_requests', owner=owner)

        def get_conversion_requests(self, owner):
            return self.__send('get_conversion_requests', owner=owner)

        def get_encrypted_memo(self, from_, to, memo):
            return self.__send('get_encrypted_memo', from_=from_, to=to, memo=memo)

        def get_feed_history(self):
            return self.__send('get_feed_history')

        def get_open_orders(self, accountname):
            return self.__send('get_open_orders', accountname=accountname)

        def get_ops_in_block(self, block_num, only_virtual):
            return self.__send('get_ops_in_block', block_num=block_num, only_virtual=only_virtual)

        def get_order_book(self, limit):
            return self.__send('get_order_book', limit=limit)

        def get_owner_history(self, account):
            return self.__send('get_owner_history', account=account)

        def get_private_key(self, pubkey):
            return self.__send('get_private_key', pubkey=pubkey)

        def get_private_key_from_password(self, account, role, password):
            return self.__send('get_private_key_from_password', account=account, role=role, password=password)

        def get_prototype_operation(self, operation_type):
            return self.__send('get_prototype_operation', operation_type=operation_type)

        def get_state(self, url):
            return self.__send('get_state', url=url)

        def get_transaction(self, trx_id):
            return self.__send('get_transaction', trx_id=trx_id)

        def get_withdraw_routes(self, account, type_):
            return self.__send('get_withdraw_routes', account=account, type=type_)

        def get_witness(self, owner_account):
            return self.__send('get_witness', owner_account=owner_account)

        def gethelp(self, method_name):
            return self.__send('gethelp', method_name=method_name)

        def help(self):
            return self.__send('help')

        def import_key(self, wif_key):
            return self.__send('import_key', wif_key=wif_key)

        def info(self):
            return self.__send('info')

        def is_locked(self):
            return self.__send('is_locked')

        def is_new(self):
            return self.__send('is_new')

        def list_accounts(self, lowerbound, limit):
            return self.__send('list_accounts', lowerbound=lowerbound, limit=limit)

        def list_keys(self):
            return self.__send('list_keys')

        def list_my_accounts(self, keys):
            return self.__send('list_my_accounts', keys=keys)

        def list_proposal_votes(self, start, limit, order_by, order_type, status):
            return self.__send('list_proposal_votes', start=start, limit=limit, order_by=order_by,
                               order_type=order_type, status=status)

        def list_proposals(self, start, limit, order_by, order_type, status):
            return self.__send('list_proposals', start=start, limit=limit, order_by=order_by, order_type=order_type,
                               status=status)

        def list_witnesses(self, lowerbound, limit):
            return self.__send('list_witnesses', lowerbound=lowerbound, limit=limit)

        def load_wallet_file(self, wallet_filename):
            return self.__send('load_wallet_file', wallet_filename=wallet_filename)

        def lock(self):
            return self.__send('lock')

        def normalize_brain_key(self, s):
            return self.__send('normalize_brain_key', s=s)

        def post_comment(self, author, permlink, parent_author, parent_permlink, title, body, json, broadcast=None):
            return self.__send('post_comment', author=author, permlink=permlink, parent_author=parent_author,
                               parent_permlink=parent_permlink, title=title, body=body, json=json, broadcast=broadcast)

        def publish_feed(self, witness, exchange_rate, broadcast=None):
            return self.__send('publish_feed', witness=witness, exchange_rate=exchange_rate, broadcast=broadcast)

        def recover_account(self, account_to_recover, recent_authority, new_authority, broadcast=None):
            return self.__send('recover_account', account_to_recover=account_to_recover,
                               recent_authority=recent_authority, new_authority=new_authority, broadcast=broadcast)

        def recurrent_transfer(self, from_, to, amount, memo, recurrence, executions, broadcast=None):
            return self.__send('recurrent_transfer', from_=from_, to=to, amount=amount, memo=memo,
                               recurrence=recurrence, executions=executions, broadcast=broadcast)

        def remove_proposal(self, deleter, ids, broadcast=None):
            return self.__send('remove_proposal', deleter=deleter, ids=ids, broadcast=broadcast)

        def request_account_recovery(self, recovery_account, account_to_recover, new_authority, broadcast=None):
            return self.__send('request_account_recovery', recovery_account=recovery_account,
                               account_to_recover=account_to_recover, new_authority=new_authority, broadcast=broadcast)

        def save_wallet_file(self, wallet_filename):
            return self.__send('save_wallet_file', wallet_filename=wallet_filename)

        def serialize_transaction(self, tx):
            return self.__send('serialize_transaction', tx=tx)

        def set_password(self, password):
            return self.__send('set_password', password=password)

        def set_transaction_expiration(self, seconds):
            return self.__send('set_transaction_expiration', seconds=seconds)

        def set_voting_proxy(self, account_to_modify, proxy, broadcast=None):
            return self.__send('set_voting_proxy', account_to_modify=account_to_modify, proxy=proxy,
                               broadcast=broadcast)

        def set_withdraw_vesting_route(self, from_, to, percent, auto_vest, broadcast=None):
            return self.__send('set_withdraw_vesting_route', from_=from_, to=to, percent=percent, auto_vest=auto_vest,
                               broadcast=broadcast)

        def sign_transaction(self, tx, broadcast=None):
            return self.__send('sign_transaction', tx=tx, broadcast=broadcast)

        def suggest_brain_key(self):
            return self.__send('suggest_brain_key')

        def transfer(self, from_, to, amount, memo, broadcast=None):
            return self.__send('transfer', from_=from_, to=to, amount=amount, memo=memo, broadcast=broadcast)

        def transfer_from_savings(self, from_, request_id, to, amount, memo, broadcast=None):
            return self.__send('transfer_from_savings', from_=from_, request_id=request_id, to=to, amount=amount,
                               memo=memo, broadcast=broadcast)

        def transfer_nonblocking(self, from_, to, amount, memo, broadcast=None):
            return self.__send('transfer_nonblocking', from_=from_, to=to, amount=amount, memo=memo,
                               broadcast=broadcast)

        def transfer_to_savings(self, from_, to, amount, memo, broadcast=None):
            return self.__send('transfer_to_savings', from_=from_, to=to, amount=amount, memo=memo, broadcast=broadcast)

        def transfer_to_vesting(self, from_, to, amount, broadcast=None):
            return self.__send('transfer_to_vesting', from_=from_, to=to, amount=amount, broadcast=broadcast)

        def transfer_to_vesting_nonblocking(self, from_, to, amount, broadcast=None):
            return self.__send('transfer_to_vesting_nonblocking', from_=from_, to=to, amount=amount,
                               broadcast=broadcast)

        def unlock(self, password):
            return self.__send('unlock', password=password)

        def update_account(self, accountname, json_meta, owner, active, posting, memo, broadcast=None):
            return self.__send('update_account', accountname=accountname, json_meta=json_meta, owner=owner,
                               active=active, posting=posting, memo=memo, broadcast=broadcast)

        def update_account_auth_account(self, account_name, type_, auth_account, weight, broadcast=None):
            return self.__send('update_account_auth_account', account_name=account_name, type=type_,
                               auth_account=auth_account, weight=weight, broadcast=broadcast)

        def update_account_auth_key(self, account_name, type_, key, weight, broadcast=None):
            return self.__send('update_account_auth_key', account_name=account_name, type=type_, key=key, weight=weight,
                               broadcast=broadcast)

        def update_account_auth_threshold(self, account_name, type_, threshold, broadcast=None):
            return self.__send('update_account_auth_threshold', account_name=account_name, type=type_,
                               threshold=threshold, broadcast=broadcast)

        def update_account_memo_key(self, account_name, key, broadcast=None):
            return self.__send('update_account_memo_key', account_name=account_name, key=key, broadcast=broadcast)

        def update_account_meta(self, account_name, json_meta, broadcast=None):
            return self.__send('update_account_meta', account_name=account_name, json_meta=json_meta,
                               broadcast=broadcast)

        def update_proposal(self, proposal_id, creator, daily_pay, subject, permlink, end_date, broadcast=None):
            return self.__send('update_proposal', proposal_id=proposal_id, creator=creator, daily_pay=daily_pay,
                               subject=subject, permlink=permlink, end_date=end_date, broadcast=broadcast)

        def update_proposal_votes(self, voter, proposals, approve, broadcast=None):
            return self.__send('update_proposal_votes', voter=voter, proposals=proposals, approve=approve,
                               broadcast=broadcast)

        def update_witness(self, witness_name, url, block_signing_key, props, broadcast=None):
            return self.__send('update_witness', witness_name=witness_name, url=url,
                               block_signing_key=block_signing_key, props=props, broadcast=broadcast)

        def vote(self, voter, author, permlink, weight, broadcast=None):
            return self.__send('vote', voter=voter, author=author, permlink=permlink, weight=weight,
                               broadcast=broadcast)

        def vote_for_witness(self, account_to_vote_with, witness_to_vote_for, approve, broadcast=None):
            return self.__send('vote_for_witness', account_to_vote_with=account_to_vote_with,
                               witness_to_vote_for=witness_to_vote_for, approve=approve, broadcast=broadcast)

        def withdraw_vesting(self, from_, vesting_shares, broadcast=None):
            return self.__send('withdraw_vesting', from_=from_, vesting_shares=vesting_shares, broadcast=broadcast)

    def __init__(self, *, attach_to: Optional['Node']):
        super().__init__()

        self.api = Wallet.__Api(self)
        self.http_server_port = None
        self.connected_node: Optional['Node'] = attach_to
        self.password = None

        if self.connected_node:
            self.name = context.names.register_numbered_name(f'{self.connected_node}.Wallet')
            self.directory = self.connected_node.directory.parent / self.name
        else:
            self.name = context.names.register_numbered_name('Wallet')
            self.directory = context.get_current_directory() / self.name

        self.executable_file_path = None
        self.stdout_file = None
        self.stderr_file = None
        self.process = None
        self.logger = logger.create_child_logger(self.name)

        self.run(timeout=15)

    def __str__(self):
        return self.name

    def __repr__(self):
        return str(self)

    def get_stdout_file_path(self):
        return self.directory / 'stdout.txt'

    def get_stderr_file_path(self):
        return self.directory / 'stderr.txt'

    def is_running(self):
        if not self.process:
            return False

        return self.process.poll() is None

    def __is_ready(self):
        with open(self.get_stderr_file_path()) as file:
            for line in file:
                if 'Entering Daemon Mode, ^C to exit' in line:
                    return True

        return False

    def run(self, timeout):
        if not self.executable_file_path:
            self.executable_file_path = paths_to_executables.get_path_of('cli_wallet')

        if not self.connected_node:
            raise Exception('Server websocket RPC endpoint not set, use Wallet.connect_to method')

        if not self.http_server_port:
            self.http_server_port = 0

        shutil.rmtree(self.directory, ignore_errors=True)
        self.directory.mkdir(parents=True)

        # pylint: disable=consider-using-with
        # Files opened here have to exist longer than current scope
        self.stdout_file = open(self.get_stdout_file_path(), 'w')
        self.stderr_file = open(self.get_stderr_file_path(), 'w')

        if not self.connected_node.is_ws_listening():
            self.logger.info(f'Waiting for node {self.connected_node} to listen...')

        timeout -= wait_for(
            self.connected_node.is_ws_listening,
            timeout=timeout,
            timeout_error_message=f'{self} waited too long for {self.connected_node} to start listening on ws port'
        )

        # pylint: disable=consider-using-with
        # Process created here have to exist longer than current scope
        self.process = subprocess.Popen(
            [
                str(self.executable_file_path),
                '-s', f'ws://{self.connected_node.get_ws_endpoint()}',
                '-d',
                '-H', f'0.0.0.0:{self.http_server_port}',
                '--rpc-http-allowip=127.0.0.1'
            ],
            cwd=self.directory,
            stdout=self.stdout_file,
            stderr=self.stderr_file
        )

        timeout -= wait_for(
            self.__is_ready,
            timeout=timeout,
            timeout_error_message=f'{self} was not ready on time.',
            poll_time=0.1
        )

        endpoint = self.__get_http_server_endpoint()
        self.http_server_port = endpoint.split(':')[1]

        timeout -= wait_for(
            self.__is_communication_established,
            timeout=timeout,
            timeout_error_message=f'Problem with starting wallet. See {self.get_stderr_file_path()} for more details.'
        )

        password = 'password'
        self.api.set_password(password)
        self.api.unlock(password)
        self.api.import_key(Account('initminer').private_key)

        self.logger.info(f'Started, listening on {endpoint}')

    def __is_communication_established(self):
        try:
            self.api.info()
        except CommunicationError:
            return False
        return True

    def __get_http_server_endpoint(self):
        with open(self.directory / 'stderr.txt') as output:
            for line in output:
                if 'Listening for incoming HTTP RPC requests on' in line:
                    endpoint = re.match(r'^.*Listening for incoming HTTP RPC requests on ([\d\.]+\:\d+)\s*$', line)[1]
                    return endpoint.replace('0.0.0.0', '127.0.0.1')
        return None

    def connect_to(self, node):
        self.connected_node = node

    def at_exit_from_scope(self):
        self.close()

    def close(self):
        self.__close_process()
        self.__close_opened_files()

    def __close_process(self):
        if self.process is None:
            return

        self.process.send_signal(signal.SIGINT)
        try:
            return_code = self.process.wait(timeout=3)
            self.logger.debug(f'Closed with {return_code} return code')
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
            self.logger.warning('Process was force-closed with SIGKILL, because didn\'t close before timeout')

    def __close_opened_files(self):
        for file in [self.stdout_file, self.stderr_file]:
            if file is not None:
                file.close()

    def set_executable_file_path(self, executable_file_path):
        self.executable_file_path = executable_file_path

    def set_http_server_port(self, port):
        self.http_server_port = port

    def send(self, method, *params, jsonrpc='2.0', id_=0):
        endpoint = f'http://127.0.0.1:{self.http_server_port}'
        message = {
            'jsonrpc': jsonrpc,
            'id': id_,
            'method': method,
            'params': list(params)
        }

        return communication.request(endpoint, message)

    def in_single_transaction(self, *, broadcast=None):
        return SingleTransactionContext(self, broadcast=broadcast)


class SingleTransactionContext:
    def __init__(self, wallet_: Wallet, *, broadcast):
        self.__wallet = wallet_
        self.__broadcast = broadcast
        self.__response = None
        self.__was_run_as_context_manager = False

    def get_response(self):
        return self.__response

    def __del__(self):
        if not self.__was_run_as_context_manager:
            raise RuntimeError(
                f'You used {Wallet.__name__}.{Wallet.in_single_transaction.__name__}() not in "with" statement'
            )

    def __enter__(self):
        self.__wallet.api._start_gathering_operations_for_single_transaction()
        self.__was_run_as_context_manager = True
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.__response = self.__wallet.api._send_gathered_operations_as_single_transaction(broadcast=self.__broadcast)
