#!/usr/bin/python3

import os

from os.path import join
from yaml import load, Loader
from json import dump
from requests import post
from sys import argv

if len(argv) < 2 or '-h' in argv or '--help' in argv:
	print("not enough arguments. Example usage: ./refresh_patterns.py http://localhost:8091")
	exit(1)

def load_yaml(filename : str) -> dict:
	with open(filename, 'rt') as file:
		while not '!include' in file.readline(): pass
		return load(file, Loader)

def create_pattern(url : str, tav_file : str, directory : str):
	# PATTERN_FILE = tav_file.split('.')[0] + '.pat.json.xxx'
	PATTERN_FILE = tav_file.split('.')[0] + '.pat.json'
	TAVERN_FILE = join(directory, tav_file)
	OUTPUT_PATTERN_FILE = join(directory, f'{PATTERN_FILE}')

	test_options = load_yaml(TAVERN_FILE)
	request = test_options['stages'][0]['request']
	output = post(url, json=request['json'], headers=request['headers'])
	assert output.status_code == 200
	parsed = output.json()

	if '_negative' in directory:
		assert 'error' in parsed, f'while processing {TAVERN_FILE}, no "error" found in result: {parsed}'
		parsed = parsed['error']
	else:
		assert 'result' in parsed, f'while processing {TAVERN_FILE}, no "result" found in result: {parsed}' + '\n' + f'{request}'
		parsed = parsed['result']

	with open(OUTPUT_PATTERN_FILE, 'wt') as file:
		dump(parsed, file, indent=2, sort_keys=True)

URL = argv[1] # Ex. 'http://localhost:8091'
from concurrent.futures import ProcessPoolExecutor

futures = []
with ProcessPoolExecutor(max_workers=6) as exec:
	for parent_path, _, filenames in os.walk('.'):
		if 'tavern' in parent_path and 'account_history_api' in parent_path:
			for tavernfile in filter(lambda x: x.endswith('tavern.yaml'), filenames):
				futures.append( exec.submit(create_pattern, URL, tavernfile, parent_path) )

	for future in futures:
		future.result()
