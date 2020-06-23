#!/usr/bin/python3.6

import sys
import os

INDENT = int(sys.argv[1])
PATH = ""
FILENAME = os.path.split( PATH )[1]
TMP_FILENAME = "____TEMPORARY_FILE_____"

INDENTATION_CHARACHTERS = [ '\t', ' ' ]
FILE_INDENTATION : int = None

DO_NOT_TOUCH_FOLDERS = [
	"libraries/fc",
	"libraries/vendor",
	"libraries/net",
	"build",
	"out",
	".git",
	".vs"
]


INPUT_LINE : str = None

def has_indent( line ):
	if len(line) == 0:
		return False

	return line[0] in INDENTATION_CHARACHTERS

def detect_indentation():
	first = None
	second = None
	with open( PATH, 'r' ) as INPUT:
		for line in INPUT:
			if has_indent(line) and first is not None:
				second = count_indents(line)
				if second > first:
					return second - first
			if len( line ) > 1:
				first = None
				second = None
			if line[ len( line ) - 2 ] == "{":
				first = count_indents(line)
	return -1

def count_indents( line ):
	count = 0
	for char in line:
		if char in INDENTATION_CHARACHTERS:
			count += 1
		else:
			return count

def process( line ):
	count = count_indents( line )
	ret = line[ count : ]
	if count % FILE_INDENTATION != 0:
		count = int( count / FILE_INDENTATION ) + 1
	else:
		count = int( count / FILE_INDENTATION )
	
	return str( " " * count * INDENT ) + ret

def processable( line : str ):
	for var in DO_NOT_TOUCH_FOLDERS:
		if line.find( var ) != -1:
			print("{}: {}".format(var, line))
			return False
	return True

i = 0
# gather input files
os.system( 'find $PWD -type f | grep -E ".+\.[hc]((pp|xx|c|f)?)$" > ____list_of_files' )
with open( "____list_of_files", 'r') as list_of_files:
	for file_to_process in list_of_files:
		fname = file_to_process[:-1] # remove \n
		# fname = fname.replace("./", os.getcwd() + "/")
		if processable( fname ):

			PATH = fname
			FILENAME = os.path.split( PATH )[1]

			# detect
			FILE_INDENTATION = detect_indentation()
			if FILE_INDENTATION == -1:
				print( "cannot determine indentation: {}; skipping".format(PATH) )
				continue
			if FILE_INDENTATION == INDENT:
				print( "nothing to change: {}; skipping".format(PATH) )
				continue
			if FILE_INDENTATION != 3:
				print( "unusual indentation of {}: {}".format(FILE_INDENTATION, PATH) )

			# process
			with open( PATH, 'r' ) as INPUT:
				with open( TMP_FILENAME, 'w' ) as OUTPUT:
					for line in INPUT:
						OUTPUT.write( process( line ) )

			print("succesfully updated indent: {}".format(PATH))

			# save
			import shutil
			shutil.move( TMP_FILENAME, PATH )

os.remove("____list_of_files")
