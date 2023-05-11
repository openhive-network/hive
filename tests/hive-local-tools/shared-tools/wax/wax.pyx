# distutils: language = c++
from libcpp.string cimport string

from wax cimport result, protocol

def validate_operation(operation: bytes):
    cdef protocol obj
    try:
        return obj.cpp_validate_operation(operation) == 1
    except Exception as ex:
        print(f'{ex}')
        return False

def validate_transaction(transaction: bytes):
    cdef protocol obj
    try:
        return obj.cpp_validate_transaction(transaction) == 1
    except Exception as ex:
        print(f'{ex}')
        return False

def calculate_digest(transaction: bytes, chain_id: bytes):
    cdef protocol obj
    try:
        result = obj.cpp_calculate_digest(transaction, chain_id)
        return result.value == 1, result.content
    except Exception as ex:
        print(f'{ex}')
        return False

def serialize_transaction(transaction: bytes):
    cdef protocol obj
    try:
        result = obj.cpp_serialize_transaction(transaction)
        return result.value == 1, result.content
    except Exception as ex:
        print(f'{ex}')
        return False