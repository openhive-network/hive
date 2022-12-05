import json
import os
import subprocess


def generate_sig_digest(transaction: dict, private_key: str):
    # sig_digest is mix of chain_id and transaction data. To get it, it's necessary to run sign_transaction executable file
    output = subprocess.check_output([os.environ['SIGN_TRANSACTION_PATH']],
                                     input=f'{{ "tx": {json.dumps(transaction)}, '
                                           f'"wif": "{private_key}" }}'.encode(encoding='utf-8')).decode('utf-8')
    return json.loads(output)['result']['sig_digest']
