#include <iostream>
#include <cstring>
#include "cpp_functions.hpp"

int multiplication(int int_param, int float_param)
{
  return int_param * float_param;
}

unsigned int get_sum_from_int_array( unsigned int* arr, unsigned int size )
{
	unsigned int sum = 0;

	for( unsigned int i = 0; i < size; ++i )
		sum += arr[i];

	return sum;
}

unsigned int get_str_length( char* str )
{
	return strlen( str );
}
