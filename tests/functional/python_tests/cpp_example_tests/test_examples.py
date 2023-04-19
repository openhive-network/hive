
import test_tools as tt

def test_examples(cpp):
    x = 6
    y = 2
    result_00 = cpp.py_multiplication(x, y)
    assert x * y == result_00

    result_01 = cpp.py_get_sum_from_int_array()
    assert result_01 == 42

    items = [100, 202, 303, 404]
    result_02 = cpp.py_get_sum_from_int_array_2(items)
    assert result_02 == 1009

    items = [66, 67, 68, 69]
    result_03 = cpp.py_get_sum_from_int_array_3(items)
    assert result_03 == 270

    str = b'abcx!'
    result_04 = cpp.py_get_str_length(str)
    assert result_04 == 5

