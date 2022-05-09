import os
import json
import subprocess

from test_tools import Asset


# creates and account, adds hives and vests to the balance
def create_account_and_fund_it(wallet, name):
    wallet.api.create_account('initminer', name, '{}')
    wallet.api.transfer_to_vesting('initminer', name, Asset.Test(250))
    wallet.api.transfer('initminer', name, Asset.Test(200), 'blueberry')


def generate_sig_digest(trx: dict, private_key: str):
    # sig_digest is mix of chain_id and transaction data. To get it, it's necessary to run sign_transaction executable file
    os.environ['SIGN_TRANSACTION_PATH'] = '/home/dev/hive/build/programs/util/sign_transaction'
    output = subprocess.check_output([os.environ['SIGN_TRANSACTION_PATH']],
                                     input=f'{{ "tx": {json.dumps(trx)}, '
                                           f'"wif": "{private_key}" }}'.encode(encoding='utf-8')).decode('utf-8')
    output = eval(output)['result']
    return output['sig_digest']
