from __future__ import annotations

import os
import re
from argparse import ArgumentParser
from concurrent.futures import ProcessPoolExecutor
from json import dump
from os.path import join
from sys import argv

from requests import post
from yaml import safe_load

ENDPOINTS = ["get_transaction", "get_account_history", "enum_virtual_ops", "get_ops_in_block"]
DEFAULT_PATTERN = f'.*/({"|".join(ENDPOINTS)})'

engine = ArgumentParser()
engine.add_argument("URL", type=str, help="reference node, which will provide pattern data (Ex. http://localhost:8091)")
engine.add_argument(
    "-s",
    dest="SUFIX",
    type=str,
    default=None,
    help=(
        'this suffix will be attached to file extension, which is handy for fast removal of patterns (Ex. "xxx",'
        " which allows to execute command: `find . | grep 'json.xxx' | xargs rm` )"
    ),
)
engine.add_argument(
    "-p",
    "--pattern",
    dest="pattern",
    type=str,
    default=DEFAULT_PATTERN,
    help=f"this will be used as regex for path matching (default: `{DEFAULT_PATTERN}`)",
)
args = engine.parse_args(argv[1:])

URL = args.URL
SUFIX = f".{args.SUFIX}" if args.SUFIX else ""
PATTERN = re.compile(args.pattern)


def load_yaml(filename: str) -> dict:
    with open(filename) as file:
        return safe_load(file.read().replace("!", ""))


def is_negative(loaded_yaml: dict) -> bool:
    response: dict = loaded_yaml["stages"][0]["response"]["verify_response_with"]
    return "extra_kwargs" in response and response["extra_kwargs"].get("error_response", False)


def create_pattern(url: str, tav_file: str, directory: str):
    pattern_file = tav_file.split(".")[0] + ".pat.json" + SUFIX
    tavern_file = join(directory, tav_file)
    output_pattern_file = join(directory, f"{pattern_file}")
    print(f"processing: {tavern_file}")

    test_options = load_yaml(tavern_file)
    request = test_options["stages"][0]["request"]
    output = post(url, json=request["json"], headers=request["headers"])
    assert output.status_code == 200
    parsed = output.json()

    if is_negative(test_options):
        assert "error" in parsed, (
            f'while processing {tavern_file}, no "error" found in result: {parsed}' + "\n" + f"{request}"
        )
        parsed = parsed["error"]
        if "data" in parsed:
            parsed.pop("data")
    else:
        assert "result" in parsed, (
            f'while processing {tavern_file}, no "result" found in result: {parsed}' + "\n" + f"{request}"
        )
        parsed = parsed["result"]

    with open(output_pattern_file, "w") as file:
        dump(parsed, file, indent=2, sort_keys=True)
        file.write("\n")


futures = []
with ProcessPoolExecutor(max_workers=6) as exec:
    for parent_path, _, filenames in os.walk("."):
        if "tavern" in parent_path and PATTERN.match(parent_path) is not None and "_hex" not in parent_path:
            for tavernfile in filter(lambda x: x.endswith("tavern.yaml"), filenames):
                futures.append(exec.submit(create_pattern, URL, tavernfile, parent_path))

    for future in futures:
        future.result()
