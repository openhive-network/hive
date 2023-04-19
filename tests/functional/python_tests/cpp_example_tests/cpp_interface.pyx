""" Some Example cython interface definition """

from libc.stdlib cimport malloc, free

from cpython cimport array
import array

"""==========DECLARATIONS=========="""
cdef extern from "cpp_functions.hpp":
    float multiplication(int int_param, float float_param)

cdef extern from "cpp_functions.hpp":
    unsigned int get_sum_from_int_array( unsigned int* arr, unsigned int size )

cdef extern from "cpp_functions.hpp":
    unsigned int get_str_length( char* str )

"""==========multiplication=========="""
def py_multiplication( int_param, float_param ):
    return multiplication( int_param, float_param )

"""==========get_sum_from_int_array=========="""
def py_get_sum_from_int_array():
    cdef unsigned int some_int = 42
    cdef unsigned int *arr = &some_int
    return get_sum_from_int_array( arr, 1 )

"""==========get_sum_from_int_array(v2)=========="""
def py_get_sum_from_int_array_2( items ):
    cdef unsigned int* arr = <unsigned int*> malloc( len(items) * sizeof(unsigned int))
    try:
        for cnt, item in enumerate(items):
            arr[cnt] = item

        return get_sum_from_int_array( arr, len(items) )
    finally:
        free( arr )

"""==========get_sum_from_int_array(v3)=========="""
def py_get_sum_from_int_array_3( items ):
    cdef array.array proxy_arr = array.array('I', items)
    return get_sum_from_int_array( proxy_arr.data.as_uints, len(items) )


"""==========get_xxx(v3)=========="""
def py_get_str_length( str ):
    cdef array.array proxy_arr = array.array('b', str)
    proxy_arr.append(0)

    return get_str_length( proxy_arr.data.as_chars )