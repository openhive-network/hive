import time

import test_tools as tt


def get_recovery_agent(node: tt.InitNode, account_name: str, wait_for_agent=None) -> str:
    time_limit = 60
    if wait_for_agent:
        start = time.time()
        while True:
            agent = node.api.database.find_accounts(accounts=[account_name])["accounts"][0]["recovery_account"]
            delta = time.time()
            if agent == wait_for_agent:
                return agent
            if delta - start > time_limit:
                raise TimeoutError("The recovery agent you were looking for was not found within the specified time")
    else:
        return node.api.database.find_accounts(accounts=[account_name])["accounts"][0]["recovery_account"]


def get_owner_key(node: tt.InitNode, account_name: str) -> str:
    return  node.api.database.find_accounts(accounts=[account_name])["accounts"][0]["owner"]["key_auths"][0][0]


def get_authority(key):
    return {"weight_threshold": 1, "account_auths": [], "key_auths": [[key, 1]]}
