#!/usr/bin/python3

import os
import re

from os.path import join
from yaml import safe_load
from json import dump
from requests import post
from sys import argv
from argparse import ArgumentParser

DEFAULT_PATTERN = '.*/account_history_api.*'

engine = ArgumentParser()
engine.add_argument('URL', type=str, help='reference node, which will provide pattern data (Ex. http://localhost:8091)')
engine.add_argument('-s', dest='SUFIX', type=str, default=None, help='this suffix will be attached to file extension, which is handy for fast removal of patterns (Ex. "xxx", which allows to execute command: `find . | grep \'json.xxx\' | xargs rm` )')
engine.add_argument('-p', '--pattern', dest='pattern', type=str, default='.*/account_history_api/.*', help=f'this will be used as regex for path matching (default: `{DEFAULT_PATTERN}`)')
args = engine.parse_args(argv[1:])

URL = args.URL
SUFIX = f'.{args.SUFIX}' if args.SUFIX else ''
PATTERN = re.compile(args.pattern)

def load_yaml(filename : str) -> dict:
	with open(filename, 'rt') as file:
		return safe_load(file.read().replace('!', ''))

def create_pattern(url : str, tav_file : str, directory : str):
	PATTERN_FILE = tav_file.split('.')[0] + '.pat.json' + SUFIX
	TAVERN_FILE = join(directory, tav_file)
	OUTPUT_PATTERN_FILE = join(directory, f'{PATTERN_FILE}')
	print(f'processing: {TAVERN_FILE}')

	test_options = load_yaml(TAVERN_FILE)
	request = test_options['stages'][0]['request']
	output = post(url, json=request['json'], headers=request['headers'])
	assert output.status_code == 200
	parsed = output.json()

	if '_negative' in directory:
		return
	else:
		assert 'result' in parsed, f'while processing {TAVERN_FILE}, no "result" found in result: {parsed}' + '\n' + f'{request}'
		parsed = parsed['result']

	with open(OUTPUT_PATTERN_FILE, 'wt') as file:
		dump(parsed, file, indent=2, sort_keys=True)
		file.write('\n')

from concurrent.futures import ProcessPoolExecutor

futures = []
with ProcessPoolExecutor(max_workers=6) as exec:
	for parent_path, _, filenames in os.walk('.'):
		if 'tavern' in parent_path and PATTERN.match(parent_path) is not None:
			for tavernfile in filter(lambda x: x.endswith('tavern.yaml'), filenames):
				futures.append( exec.submit(create_pattern, URL, tavernfile, parent_path) )

	for future in futures:
		future.result()
