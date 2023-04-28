from cpython cimport array
import array

from typing import Tuple

cdef extern from "cpython_interface.hpp":
    int cpp_validate_operation( char* content )

def validate_operation(operation: bytes) -> bool | None:
    cdef array.array _operation = array.array('b', operation)
    _operation.append(0)

    return bool(cpp_validate_operation( _operation.data.as_chars ))

cdef extern from "cpython_interface.hpp":
    int cpp_calculate_digest( char* content, unsigned char* digest )

def calculate_digest(transaction: bytes) -> Tuple[bool, str] | None:
    cdef array.array _transaction = array.array('b', transaction)
    _transaction.append(0)

    _DIGEST_SIZE = 32
    _buffer = '0' * _DIGEST_SIZE
    __buffer = bytes(_buffer, 'ascii')

    cdef array.array _digest = array.array('B', __buffer)

    _result = bool(cpp_calculate_digest( _transaction.data.as_chars, _digest.data.as_uchars ))

    return _result, ''.join('{:02x}'.format(c) for c in _digest)

cdef extern from "cpython_interface.hpp":
    int cpp_validate_transaction( char* content )

def validate_transaction(transaction: bytes) -> bool | None:
    cdef array.array _transaction = array.array('b', transaction)
    _transaction.append(0)

    return bool(cpp_validate_transaction( _transaction.data.as_chars ))

cdef extern from "cpython_interface.hpp":
    int cpp_serialize_transaction( char* content, unsigned char* serialized_transaction )

def serialize_transaction(transaction: bytes) -> bool | None:
    cdef array.array _transaction = array.array('b', transaction)
    _transaction.append(0)

    _DIGEST_SIZE = 50000
    _buffer = '0' * _DIGEST_SIZE
    __buffer = bytes(_buffer, 'ascii')

    cdef array.array _ser_buffer = array.array('B', __buffer)

    _result = bool(cpp_serialize_transaction( _transaction.data.as_chars, _ser_buffer.data.as_uchars ))

    return _result, ''.join(str(c) for c in _ser_buffer)
