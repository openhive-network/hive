from distutils.util import strtobool
import os
from pathlib import Path
import re

from .....local_tools import read_from_json_pattern, write_to_json_pattern

__PATTERNS_DIRECTORY = Path(__file__).with_name('response_patterns')


def __read_from_text_pattern(method_name: str) -> str:
    with open(f'{__PATTERNS_DIRECTORY}/{method_name}.pat.txt', 'r') as text_file:
        return text_file.read()


def __write_to_text_pattern(method_name: str, text: str) -> None:
    __PATTERNS_DIRECTORY.mkdir(parents=True, exist_ok=True)
    with open(f'{__PATTERNS_DIRECTORY}/{method_name}.pat.txt', 'w') as text_file:
        text_file.write(text)


def split_text_to_separated_words(text):
    lines = text.splitlines()
    words = []
    for line in lines:
        words.append(re.split(r'\s{2,}', line))

    return words


def verify_json_patterns(method_name, method_output):
    """
    Asserts that `method_output` is same as corresponding pattern. Optionally updates (overrides)
    existing patterns with actual method output, if `GENERATE_PATTERNS` environment variable is set.
    """
    generate_patterns = strtobool(os.environ.get('GENERATE_PATTERNS', 'OFF'))
    if not generate_patterns:
        pattern = read_from_json_pattern(__PATTERNS_DIRECTORY, method_name)
        assert pattern == method_output

    write_to_json_pattern(__PATTERNS_DIRECTORY, method_name, method_output)


def verify_text_patterns(method_name, method_output):
    """
    Asserts that `method_output` is same as corresponding pattern. Optionally updates (overrides)
    existing patterns with actual method output, if `GENERATE_PATTERNS` environment variable is set.
    """
    generate_patterns = strtobool(os.environ.get('GENERATE_PATTERNS', 'OFF'))
    if not generate_patterns:
        pattern = __read_from_text_pattern(method_name)
        assert pattern == method_output

    __write_to_text_pattern(method_name, method_output)
