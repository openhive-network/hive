#!/usr/bin/python3
'''
This script generates a text file based on a template and values read from the environment.
Uses https://docs.python.org/3/library/string.html#template-strings to process template.
Run './generate-file-from-template.py -h' to see usage instructions.
'''
import argparse
import os
from string import Template

parser = argparse.ArgumentParser(description='Script to generate a text file based on a template and environment variables.')
parser.add_argument("-i", "--input-file", help = "Template to be processed", required=True)
parser.add_argument("-o", "--output-file", help = "File to write output to", required=True)

args = parser.parse_args()

input_file = getattr(args, "input_file")
output_file = getattr(args, "output_file")

with (
    open(input_file, 'r') as input, 
    open(output_file, 'w') as output
):
    src = Template(input.read())
    result = src.substitute(os.environ)
    output.write(result)