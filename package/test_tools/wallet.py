from pathlib import Path
import subprocess
import signal
import time

from .account import Account
from . import logger


class Wallet:
    class __Api:
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

                return response

            def get_transaction(self):
                return self.__transaction

        def __init__(self, wallet):
            self.__wallet = wallet
            self.__transaction_builder = None

        def _start_gathering_operations_for_single_transaction(self):
            if self.__transaction_builder is not None:
                raise RuntimeError('You cannot create transaction inside another transaction')

            self.__transaction_builder = self.__TransactionBuilder()

        def _send_gathered_operations_as_single_transaction(self):
            self.sign_transaction(
                self.__transaction_builder.get_transaction(),
                True
            )
            self.__transaction_builder = None

        def __send(self, method, *params, jsonrpc='2.0', id=0):
            if self.__transaction_builder is None:
                return self.__wallet.send(method, *params, jsonrpc=jsonrpc, id=id)

            return self.__transaction_builder.append_operation(
                self.__wallet.send(method, *params, jsonrpc=jsonrpc, id=id)
            )

        def about(self):
            return self.__send('about')

        def cancel_order(self, owner, orderid, broadcast):
            return self.__send('cancel_order', owner, orderid, broadcast)

        def cancel_transfer_from_savings(self, from_, request_id, broadcast):
            return self.__send('cancel_transfer_from_savings', from_, request_id, broadcast)

        def change_recovery_account(self, owner, new_recovery_account, broadcast):
            return self.__send('change_recovery_account', owner, new_recovery_account, broadcast)

        def claim_account_creation(self, creator, fee, broadcast):
            return self.__send('claim_account_creation', creator, fee, broadcast)

        def claim_account_creation_nonblocking(self, creator, fee, broadcast):
            return self.__send('claim_account_creation_nonblocking', creator, fee, broadcast)

        def claim_reward_balance(self, account, reward_hive, reward_hbd, reward_vests, broadcast):
            return self.__send('claim_reward_balance', account, reward_hive, reward_hbd, reward_vests, broadcast)

        def convert_hbd(self, from_, amount, broadcast):
            return self.__send('convert_hbd', from_, amount, broadcast)

        def convert_hive_with_collateral(self, from_, collateral_amount, broadcast):
            return self.__send('convert_hive_with_collateral', from_, collateral_amount, broadcast)

        def create_account(self, creator, new_account_name, json_meta, broadcast):
            return self.__send('create_account', creator, new_account_name, json_meta, broadcast)

        def create_account_delegated(self, creator, hive_fee, delegated_vests, new_account_name, json_meta, broadcast):
            return self.__send('create_account_delegated', creator, hive_fee, delegated_vests, new_account_name,
                               json_meta, broadcast)

        def create_account_with_keys(self, creator, newname, json_meta, owner, active, posting, memo, broadcast):
            return self.__send('create_account_with_keys', creator, newname, json_meta, owner, active, posting, memo,
                               broadcast)

        def create_account_with_keys_delegated(self, creator, hive_fee, delegated_vests, newname, json_meta, owner,
                                               active, posting, memo, broadcast):
            return self.__send('create_account_with_keys_delegated', creator, hive_fee, delegated_vests, newname,
                               json_meta, owner, active, posting, memo, broadcast)

        def create_funded_account_with_keys(self, creator, new_account_name, initial_amount, memo, json_meta, owner_key,
                                            active_key, posting_key, memo_key, broadcast):
            return self.__send('create_funded_account_with_keys', creator, new_account_name, initial_amount, memo,
                               json_meta, owner_key, active_key, posting_key, memo_key, broadcast)

        def create_order(self, owner, order_id, amount_to_sell, min_to_receive, fill_or_kill, expiration, broadcast):
            return self.__send('create_order', owner, order_id, amount_to_sell, min_to_receive, fill_or_kill,
                               expiration, broadcast)

        def create_proposal(self, creator, receiver, start_date, end_date, daily_pay, subject, permlink, broadcast):
            return self.__send('create_proposal', creator, receiver, start_date, end_date, daily_pay, subject, permlink,
                               broadcast)

        def decline_voting_rights(self, account, decline, broadcast):
            return self.__send('decline_voting_rights', account, decline, broadcast)

        def decrypt_memo(self, memo):
            return self.__send('decrypt_memo', memo)

        def delegate_vesting_shares(self, delegator, delegatee, vesting_shares, broadcast):
            return self.__send('delegate_vesting_shares', delegator, delegatee, vesting_shares, broadcast)

        def delegate_vesting_shares_and_transfer(self, delegator, delegatee, vesting_shares, transfer_amount,
                                                 transfer_memo, broadcast):
            return self.__send('delegate_vesting_shares_and_transfer', delegator, delegatee, vesting_shares,
                               transfer_amount, transfer_memo, broadcast)

        def delegate_vesting_shares_and_transfer_nonblocking(self, delegator, delegatee, vesting_shares,
                                                             transfer_amount, transfer_memo, broadcast):
            return self.__send('delegate_vesting_shares_and_transfer_nonblocking', delegator, delegatee, vesting_shares,
                               transfer_amount, transfer_memo, broadcast)

        def delegate_vesting_shares_nonblocking(self, delegator, delegatee, vesting_shares, broadcast):
            return self.__send('delegate_vesting_shares_nonblocking', delegator, delegatee, vesting_shares, broadcast)

        def escrow_approve(self, from_, to, agent, who, escrow_id, approve, broadcast):
            return self.__send('escrow_approve', from_, to, agent, who, escrow_id, approve, broadcast)

        def escrow_dispute(self, from_, to, agent, who, escrow_id, broadcast):
            return self.__send('escrow_dispute', from_, to, agent, who, escrow_id, broadcast)

        def escrow_release(self, from_, to, agent, who, receiver, escrow_id, hbd_amount, hive_amount, broadcast):
            return self.__send('escrow_release', from_, to, agent, who, receiver, escrow_id, hbd_amount, hive_amount,
                               broadcast)

        def escrow_transfer(self, from_, to, agent, escrow_id, hbd_amount, hive_amount, fee, ratification_deadline,
                            escrow_expiration, json_meta, broadcast):
            return self.__send('escrow_transfer', from_, to, agent, escrow_id, hbd_amount, hive_amount, fee,
                               ratification_deadline, escrow_expiration, json_meta, broadcast)

        def estimate_hive_collateral(self, hbd_amount_to_get):
            return self.__send('estimate_hive_collateral', hbd_amount_to_get)

        def find_proposals(self, proposal_ids):
            return self.__send('find_proposals', proposal_ids)

        def find_recurrent_transfers(self, from_):
            return self.__send('find_recurrent_transfers', from_)

        def follow(self, follower, following, what, broadcast):
            return self.__send('follow', follower, following, what, broadcast)

        def get_account(self, account_name):
            return self.__send('get_account', account_name)

        def get_account_history(self, account, from_, limit):
            return self.__send('get_account_history', account, from_, limit)

        def get_active_witnesses(self):
            return self.__send('get_active_witnesses')

        def get_block(self, num):
            return self.__send('get_block', num)

        def get_collateralized_conversion_requests(self, owner):
            return self.__send('get_collateralized_conversion_requests', owner)

        def get_conversion_requests(self, owner):
            return self.__send('get_conversion_requests', owner)

        def get_encrypted_memo(self, from_, to, memo):
            return self.__send('get_encrypted_memo', from_, to, memo)

        def get_feed_history(self):
            return self.__send('get_feed_history')

        def get_open_orders(self, accountname):
            return self.__send('get_open_orders', accountname)

        def get_ops_in_block(self, block_num, only_virtual):
            return self.__send('get_ops_in_block', block_num, only_virtual)

        def get_order_book(self, limit):
            return self.__send('get_order_book', limit)

        def get_owner_history(self, account):
            return self.__send('get_owner_history', account)

        def get_private_key(self, pubkey):
            return self.__send('get_private_key', pubkey)

        def get_private_key_from_password(self, account, role, password):
            return self.__send('get_private_key_from_password', account, role, password)

        def get_prototype_operation(self, operation_type):
            return self.__send('get_prototype_operation', operation_type)

        def get_state(self, url):
            return self.__send('get_state', url)

        def get_transaction(self, trx_id):
            return self.__send('get_transaction', trx_id)

        def get_withdraw_routes(self, account, type):
            return self.__send('get_withdraw_routes', account, type)

        def get_witness(self, owner_account):
            return self.__send('get_witness', owner_account)

        def gethelp(self, method):
            return self.__send('gethelp', method)

        def help(self):
            return self.__send('help')

        def import_key(self, wif_key):
            return self.__send('import_key', wif_key)

        def info(self):
            return self.__send('info')

        def is_locked(self):
            return self.__send('is_locked')

        def is_new(self):
            return self.__send('is_new')

        def list_accounts(self, lowerbound, limit):
            return self.__send('list_accounts', lowerbound, limit)

        def list_keys(self):
            return self.__send('list_keys')

        def list_my_accounts(self):
            return self.__send('list_my_accounts')

        def list_proposal_votes(self, start, limit, order_by, order_type, status):
            return self.__send('list_proposal_votes', start, limit, order_by, order_type, status)

        def list_proposals(self, start, limit, order_by, order_type, status):
            return self.__send('list_proposals', start, limit, order_by, order_type, status)

        def list_witnesses(self, lowerbound, limit):
            return self.__send('list_witnesses', lowerbound, limit)

        def load_wallet_file(self, wallet_filename):
            return self.__send('load_wallet_file', wallet_filename)

        def lock(self):
            return self.__send('lock')

        def normalize_brain_key(self, s):
            return self.__send('normalize_brain_key', s)

        def post_comment(self, author, permlink, parent_author, parent_permlink, title, body, json, broadcast):
            return self.__send('post_comment', author, permlink, parent_author, parent_permlink, title, body, json,
                               broadcast)

        def publish_feed(self, witness, exchange_rate, broadcast):
            return self.__send('publish_feed', witness, exchange_rate, broadcast)

        def recover_account(self, account_to_recover, recent_authority, new_authority, broadcast):
            return self.__send('recover_account', account_to_recover, recent_authority, new_authority, broadcast)

        def recurrent_transfer(self, from_, to, amount, memo, recurrence, executions, broadcast):
            return self.__send('recurrent_transfer', from_, to, amount, memo, recurrence, executions, broadcast)

        def remove_proposal(self, deleter, ids, broadcast):
            return self.__send('remove_proposal', deleter, ids, broadcast)

        def request_account_recovery(self, recovery_account, account_to_recover, new_authority, broadcast):
            return self.__send('request_account_recovery', recovery_account, account_to_recover, new_authority,
                               broadcast)

        def save_wallet_file(self, wallet_filename):
            return self.__send('save_wallet_file', wallet_filename)

        def serialize_transaction(self, tx):
            return self.__send('serialize_transaction', tx)

        def set_password(self, password):
            return self.__send('set_password', password)

        def set_transaction_expiration(self, seconds):
            return self.__send('set_transaction_expiration', seconds)

        def set_voting_proxy(self, account_to_modify, proxy, broadcast):
            return self.__send('set_voting_proxy', account_to_modify, proxy, broadcast)

        def set_withdraw_vesting_route(self, from_, to, percent, auto_vest, broadcast):
            return self.__send('set_withdraw_vesting_route', from_, to, percent, auto_vest, broadcast)

        def sign_transaction(self, tx, broadcast):
            return self.__send('sign_transaction', tx, broadcast)

        def suggest_brain_key(self):
            return self.__send('suggest_brain_key')

        def transfer(self, from_, to, amount, memo, broadcast):
            return self.__send('transfer', from_, to, amount, memo, broadcast)

        def transfer_from_savings(self, from_, request_id, to, amount, memo, broadcast):
            return self.__send('transfer_from_savings', from_, request_id, to, amount, memo, broadcast)

        def transfer_nonblocking(self, from_, to, amount, memo, broadcast):
            return self.__send('transfer_nonblocking', from_, to, amount, memo, broadcast)

        def transfer_to_savings(self, from_, to, amount, memo, broadcast):
            return self.__send('transfer_to_savings', from_, to, amount, memo, broadcast)

        def transfer_to_vesting(self, from_, to, amount, broadcast):
            return self.__send('transfer_to_vesting', from_, to, amount, broadcast)

        def transfer_to_vesting_nonblocking(self, from_, to, amount, broadcast):
            return self.__send('transfer_to_vesting_nonblocking', from_, to, amount, broadcast)

        def unlock(self, password):
            return self.__send('unlock', password)

        def update_account(self, accountname, json_meta, owner, active, posting, memo, broadcast):
            return self.__send('update_account', accountname, json_meta, owner, active, posting, memo, broadcast)

        def update_account_auth_account(self, account_name, type, auth_account, weight, broadcast):
            return self.__send('update_account_auth_account', account_name, type, auth_account, weight, broadcast)

        def update_account_auth_key(self, account_name, type, key, weight, broadcast):
            return self.__send('update_account_auth_key', account_name, type, key, weight, broadcast)

        def update_account_auth_threshold(self, account_name, type, threshold, broadcast):
            return self.__send('update_account_auth_threshold', account_name, type, threshold, broadcast)

        def update_account_memo_key(self, account_name, key, broadcast):
            return self.__send('update_account_memo_key', account_name, key, broadcast)

        def update_account_meta(self, account_name, json_meta, broadcast):
            return self.__send('update_account_meta', account_name, json_meta, broadcast)

        def update_proposal(self, proposal_id, creator, daily_pay, subject, permlink, end_date, broadcast):
            return self.__send('update_proposal', proposal_id, creator, daily_pay, subject, permlink, end_date,
                               broadcast)

        def update_proposal_votes(self, voter, proposals, approve, broadcast):
            return self.__send('update_proposal_votes', voter, proposals, approve, broadcast)

        def update_witness(self, witness_name, url, block_signing_key, props, broadcast):
            return self.__send('update_witness', witness_name, url, block_signing_key, props, broadcast)

        def vote(self, voter, author, permlink, weight, broadcast):
            return self.__send('vote', voter, author, permlink, weight, broadcast)

        def vote_for_witness(self, account_to_vote_with, witness_to_vote_for, approve, broadcast):
            return self.__send('vote_for_witness', account_to_vote_with, witness_to_vote_for, approve, broadcast)

        def withdraw_vesting(self, from_, vesting_shares, broadcast):
            return self.__send('withdraw_vesting', from_, vesting_shares, broadcast)

    def __init__(self, name, creator, directory=Path()):
        self.api = Wallet.__Api(self)
        self.http_server_port = None
        self.connected_node = None
        self.password = None

        self.creator = creator
        self.name = name
        self.directory = directory / self.name
        self.executable_file_path = None
        self.stdout_file = None
        self.stderr_file = None
        self.process = None
        self.finalizer = None
        self.logger = logger.getLogger(f'{__name__}.{self.creator}.{self.name}')

    @staticmethod
    def __close_process(process, logger_from_wallet):
        if not process:
            return

        process.send_signal(signal.SIGINT)

        try:
            return_code = process.wait(timeout=3)
            logger_from_wallet.debug(f'Closed with {return_code} return code')
        except subprocess.TimeoutExpired:
            process.send_signal(signal.SIGKILL)
            logger_from_wallet.warning(f"Send SIGKILL because process didn't close before timeout")

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

    def run(self):
        if not self.executable_file_path:
            from . import paths_to_executables
            self.executable_file_path = paths_to_executables.get_path_of('cli_wallet')

        if not self.connected_node:
            raise Exception('Server websocket RPC endpoint not set, use Wallet.connect_to method')

        if not self.http_server_port:
            from .port import Port
            self.http_server_port = Port.allocate()

        import shutil
        shutil.rmtree(self.directory, ignore_errors=True)
        self.directory.mkdir(parents=True)

        self.stdout_file = open(self.get_stdout_file_path(), 'w')
        self.stderr_file = open(self.get_stderr_file_path(), 'w')

        if not self.connected_node.is_ws_listening():
            self.logger.info(f'Waiting for node {self.connected_node} to listen...')

        while not self.connected_node.is_ws_listening():
            time.sleep(1)

        self.process = subprocess.Popen(
            [
                str(self.executable_file_path),
                '--chain-id=04e8b5fc4bb4ab3c0ee3584199a2e584bfb2f141222b3a0d1c74e8a75ec8ff39',
                '-s', f'ws://{self.connected_node.config.webserver_ws_endpoint}',
                '-d',
                '-H', f'0.0.0.0:{self.http_server_port}',
                '--rpc-http-allowip', '192.168.10.10',
                '--rpc-http-allowip=127.0.0.1'
            ],
            cwd=self.directory,
            stdout=self.stdout_file,
            stderr=self.stderr_file
        )

        import weakref
        self.finalizer = weakref.finalize(self, Wallet.__close_process, self.process, self.logger)

        while not self.__is_ready():
            time.sleep(0.1)

        from .communication import CommunicationError
        success = False
        for _ in range(30):
            try:
                self.api.info()

                success = True
                break
            except CommunicationError:
                time.sleep(1)

        if not success:
            raise Exception(f'Problem with starting wallet occurred. See {self.get_stderr_file_path()} for more details.')

        password = 'password'
        self.api.set_password(password)
        self.api.unlock(password)
        self.api.import_key(Account('initminer').private_key)

        self.logger.info(f'Started, listening on port {self.http_server_port}')

    def connect_to(self, node):
        self.connected_node = node

    def close(self):
        self.finalizer()

    def set_executable_file_path(self, executable_file_path):
        self.executable_file_path = executable_file_path

    def set_http_server_port(self, port):
        self.http_server_port = port

    def send(self, method, *params, jsonrpc='2.0', id=0):
        endpoint = f'http://127.0.0.1:{self.http_server_port}'
        message = {
            'jsonrpc': jsonrpc,
            'id': id,
            'method': method,
            'params': list(params)
        }

        from . import communication
        return communication.request(endpoint, message)

    def create_account(self, name, creator='initminer'):
        account = Account(name)

        self.api.create_account_with_keys(creator, account.name)
        self.api.import_key(account.private_key)

        return account

    def in_single_transaction(self):
        return SingleTransactionContext(self)


class SingleTransactionContext:
    def __init__(self, wallet_: Wallet):
        self.__wallet = wallet_
        self.__was_run_as_context_manager = False

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
        self.__wallet.api._send_gathered_operations_as_single_transaction()
