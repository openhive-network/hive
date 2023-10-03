import json
from datetime import datetime

import test_tools as tt
from hive_local_tools.constants import ALTERNATE_CHAIN_JSON_FILENAME


def change_hive_owner_update_limit(seconds_limit: int) -> None:
    current_time = datetime.now()
    alternate_chain_spec_content = {
        "genesis_time": int(current_time.timestamp()),
        "hardfork_schedule": [{"hardfork": 26, "block_num": 1}],
        "hive_owner_update_limit": seconds_limit,
    }

    directory = tt.context.get_current_directory()
    directory.mkdir(parents=True, exist_ok=True)
    with open(directory / ALTERNATE_CHAIN_JSON_FILENAME, "w") as json_file:
        json.dump(alternate_chain_spec_content, json_file)
