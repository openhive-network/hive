#!/bin/bash

HIVE_SOURCE_DIR="$1"
GET_CONFIG_CPP_PATH="$HIVE_SOURCE_DIR/libraries/protocol/get_config.cpp"
CONFIG_HPP_PATH="$HIVE_SOURCE_DIR/libraries/protocol/include/hive/protocol/config.hpp"

DIFF_1="/tmp/diff.$RANDOM.$RANDOM.$RANDOM"
DIFF_2="/tmp/diff.$RANDOM.$RANDOM.$RANDOM"

# get dumps from get_config.cpp
cat $GET_CONFIG_CPP_PATH | grep "result\[" | cut -d '=' -f 2 | tr -d ' ;' | grep -E '^[A-Z]' | sort -u > $DIFF_1;

# get #define from config.hpp
cat $CONFIG_HPP_PATH | grep -E "^#define" | cut -d ' ' -f 2 | tr -d ' ;' | sort -u > $DIFF_2;

# if number o lines are equal 0, there is no diffrences
ret_count=$(diff $DIFF_1 $DIFF_2 | wc -l)

# print missmatches
if [ $ret_count != 0 ] 
	then
		echo "missmatch between $GET_CONFIG_CPP_PATH and $GET_CONFIG_CPP_PATH:"
		diff $DIFF_1 $DIFF_2
fi

rm $DIFF_1 $DIFF_2
exit $ret_count