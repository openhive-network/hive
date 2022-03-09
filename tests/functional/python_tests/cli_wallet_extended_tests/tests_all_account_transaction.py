import pytest

from all_account_transaction import compare_transactions, assign_transactions_keys


def test_different_transactions():
    dataA = [
        0,
        {
            "trx_id": "diff dataA",
            "block": 2794856,
            "trx_in_block": 0,
            "op_in_trx": 0,
            "virtual_op": 1,
            "timestamp": "2016-06-30T17:22:18",
            "op": {
                "type": "account_created_operation",
                "value": {
                    "new_account_name": "gtg",
                    "creator": "",
                    "initial_vesting_shares": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    },
                    "initial_delegation": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    }
                }
            },
            "operation_id": 0
        }
    ]

    dataB = [
        0,
        {
            "trx_id": "trzecia transakcja nr0, taki sam klucz",
            "block": 2794856,
            "trx_in_block": 0,
            "op_in_trx": 0,
            "virtual_op": 1,
            "timestamp": "2016-06-30T17:22:18",
            "op": {
                "type": "account_created_operation",
                "value": {
                    "new_account_name": "gtg",
                    "creator": "",
                    "initial_vesting_shares": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    },
                    "initial_delegation": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    }
                }
            },
            "operation_id": 0
        }
    ]
    assert compare_transactions('develop', 'test', [dataA], [dataB]) != [dataB]


def test_same_transactions():
    data = [
        0,
        {
            "trx_id": "trzecia transakcja nr0, taki sam klucz",
            "block": 2794856,
            "trx_in_block": 0,
            "op_in_trx": 0,
            "virtual_op": 1,
            "timestamp": "2016-06-30T17:22:18",
            "op": {
                "type": "account_created_operation",
                "value": {
                    "new_account_name": "gtg",
                    "creator": "",
                    "initial_vesting_shares": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    },
                    "initial_delegation": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    }
                }
            },
            "operation_id": 0
        }
    ]
    assert compare_transactions('develop', 'test', [data], [data]) == []


def test_different_key():
    dataA = [
        0,
        {
            "trx_id": "data",
            "block": 111111,
            "trx_in_block": 0,
            "op_in_trx": 0,
            "virtual_op": 1,
            "timestamp": "2016-06-30T17:22:18",
            "op": {
                "type": "account_created_operation",
                "value": {
                    "new_account_name": "gtg",
                    "creator": "",
                    "initial_vesting_shares": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    },
                    "initial_delegation": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    }
                }
            },
            "operation_id": 0
        }
    ]

    dataB = [
        0,
        {
            "trx_id": "data",
            "block": 222222,
            "trx_in_block": 0,
            "op_in_trx": 0,
            "virtual_op": 1,
            "timestamp": "2016-06-30T17:22:18",
            "op": {
                "type": "account_created_operation",
                "value": {
                    "new_account_name": "gtg",
                    "creator": "",
                    "initial_vesting_shares": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    },
                    "initial_delegation": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    }
                }
            },
            "operation_id": 0
        }
    ]
    assert compare_transactions('develop', 'test', [dataA], [dataB]) != [dataB]


def test_zero_false_problem():
    transacrion_number_false = [
        0,
        {
            "trx_id": "data",
            "block": 111111,
            "trx_in_block": 0,
            "op_in_trx": 0,
            "virtual_op": 1,
            "timestamp": "2016-06-30T17:22:18",
            "op": {
                "type": "account_created_operation",
                "value": {
                    "new_account_name": "gtg",
                    "creator": "",
                    "initial_vesting_shares": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    },
                    "initial_delegation": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    }
                }
            },
            "operation_id": 0
        }
    ]

    transaction_number_zero = [
        0,
        {
            "trx_id": "data",
            "block": 111111,
            "trx_in_block": False,
            "op_in_trx": 0,
            "virtual_op": 1,
            "timestamp": "2016-06-30T17:22:18",
            "op": {
                "type": "account_created_operation",
                "value": {
                    "new_account_name": "gtg",
                    "creator": "",
                    "initial_vesting_shares": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    },
                    "initial_delegation": {
                        "amount": "0",
                        "precision": 6,
                        "nai": "@@000000037"
                    }
                }
            },
            "operation_id": 0
        }
    ]
    assert compare_transactions('develop', 'test', [transacrion_number_false], [transaction_number_zero]) == [transaction_number_zero]
