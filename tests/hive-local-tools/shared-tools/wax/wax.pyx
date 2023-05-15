# distutils: language = c++
from libcpp.string cimport string

from wax cimport error_code, result, protocol

def validate_operation(operation: bytes):
    cdef protocol obj
    try:
        result = obj.cpp_validate_operation(operation)
        return result.value == error_code.ok, result.exception_message
    except Exception as ex:
        print(f'{ex}')
        return False, result.content, result.exception_message

def validate_transaction(transaction: bytes):
    cdef protocol obj
    try:
        result = obj.cpp_validate_transaction(transaction)
        return result.value == error_code.ok, result.exception_message
    except Exception as ex:
        print(f'{ex}')
        return False, result.content, result.exception_message

def calculate_digest(transaction: bytes, chain_id: bytes):
    cdef protocol obj
    try:
        result = obj.cpp_calculate_digest(transaction, chain_id)
        return result.value == error_code.ok, result.content, result.exception_message
    except Exception as ex:
        print(f'{ex}')
        return False, result.content, result.exception_message

def serialize_transaction(transaction: bytes):
    cdef protocol obj
    try:
        result = obj.cpp_serialize_transaction(transaction)
        return result.value == error_code.ok, result.content, result.exception_message
    except Exception as ex:
        print(f'{ex}')
        return False, result.content, result.exception_message
