from libcpp.string cimport string

cdef extern from "cpython_interface.hpp" namespace "cpp":
    cdef cppclass result:
        unsigned int value
        string content

    cdef cppclass protocol:
        unsigned int cpp_validate_operation( string operation )
        unsigned int cpp_validate_transaction( string transaction )
        result cpp_calculate_digest( string transaction, string chain_id )
        result cpp_serialize_transaction( string transaction )