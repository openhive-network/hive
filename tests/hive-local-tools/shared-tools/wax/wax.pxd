from libcpp.string cimport string

cdef extern from "cpython_interface.hpp" namespace "cpp":
    cdef enum error_code:
        fail
        ok

    cdef cppclass result:
        error_code value
        string content
        string exception_message

    cdef cppclass protocol:
        result cpp_validate_operation( string operation )
        result cpp_validate_transaction( string transaction )
        result cpp_calculate_digest( string transaction, string chain_id )
        result cpp_serialize_transaction( string transaction )