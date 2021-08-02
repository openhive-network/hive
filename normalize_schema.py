#!/usr/bin/python3.6

'''

	This script will format `schema.sql` by deleting all new lines, comments and blank lines

	WARNING:	This is very simple script, and will fail with more advanced statements like functions,
						so You will have to format them by yourself

	Example usage:

		./normalize_schema.py ./schema.sql schema.sql.norm

'''

import sys

assert len(sys.argv) == 3, 'not enough arguments'
INPUT_FILE = sys.argv[1]
OUTPUT_FILE = sys.argv[2]

data = list()
with open(INPUT_FILE, 'r') as file:
	for line in file:
		data.append( str(line) )

is_complex = None
current_querry = ''
ready_queries = list()

def prepare_line_to_analyze( line : str ) -> str:
	return line.strip()

def is_comment( line : str ) -> bool:
	return line.startswith('--')

def is_blank( line : str ) -> bool:
	return len(line) == 0

def is_end_line( line : str ) -> bool:
	return line.endswith(';')

def prepare_line_to_append( line : str ) -> str:
	return f'{ "" if len(current_querry) == 0 else " " }{line}'

for _line in data:
	line = prepare_line_to_analyze(_line)
	
	if is_comment(line) or is_blank(line):
		continue

	current_querry += prepare_line_to_append(line)

	if is_end_line(line):
		ready_queries.append(f"{ current_querry.replace(';', '') }\n")
		current_querry = ''

with open(OUTPUT_FILE, 'w') as file:
	file.writelines(ready_queries)
