
cpdef void info(str param):
    print("This is info: {}".format(param))

cdef extern from "cpython_interface.hpp":
    int add_three_assets( int param_1, int param_2, int param_3 )

def py_add_three_assets( param_1, param_2, param_3 ):
    return add_three_assets( param_1, param_2, param_3 )

cdef extern from "cpython_interface.hpp":
    int add_two_numbers( int param_1, int param_2 )

def py_add_two_numbers( param_1, param_2 ):
    return add_two_numbers( param_1, param_2 )
