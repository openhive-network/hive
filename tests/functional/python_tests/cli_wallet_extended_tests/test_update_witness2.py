import json
import os
import subprocess

import test_tools as tt


def test_witness_update2(wallet):
    initminer = wallet.api.get_account('initminer')
    initminer_posting_key = initminer['posting']['key_auths'][0][0]

    serialized_posting_key = serialize('key', initminer_posting_key)
    serialized_asset = serialize('account_creation_fee', tt.Asset.Test(3).as_nai())

    props = [["account_creation_fee", serialized_asset], ["key", serialized_posting_key]]

    assert wallet.api.get_witness('initminer')['props']['account_creation_fee'] != tt.Asset.Test(3)

    wallet.api.update_witness2('initminer', props, True)

    assert wallet.api.get_witness('initminer')['props']['account_creation_fee'] == tt.Asset.Test(3)


def serialize(object_to_serialize, value: str) -> str:
    object_to_serialize = json.dumps(object_to_serialize)
    value = json.dumps(value)
    output = subprocess.check_output([os.environ['SERIALIZE_SET_PROPERTIES_PATH']],
                                     input=f'{{{object_to_serialize}:{value}}}'.encode(encoding='utf-8')).decode('utf-8')
    serialized_value = json.loads(output)[0][1]
    return serialized_value
