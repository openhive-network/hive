import json
import re
from os import getenv, remove
from pathlib import Path
from typing import List, Literal, Tuple, Union

import pytest
import test_tools as tt
from local_tools import Config, are_objects_same, gather_all_tests, get_response, remove_tag, save_json

# set of predefined "tags" that can be used in tests to ignore specific elements
# of result json when comparing with pattern - only one per test (ignore_tags should
# be a string denoting predefined situation), exclusive with regular (not predefined)
# tags (in normal case ignore_tags must be a list of tag specifiers)
PREDEFINED_IGNORE_TAGS = {
    '<error>': re.compile(r"root\['data'\]"),
    '<bridge post>' : re.compile(r"root\['post_id'\]"),
    '<bridge posts>' : re.compile(r"root\[\d+\]\['post_id'\]"),
    '<bridge discussion>' : re.compile(r"root\[.+\]\['post_id'\]"),
    '<bridge community>' : re.compile(r"root\['id'\]"),
    '<bridge communities>' : re.compile(r"root\[\d+\]\['id'\]"),
    '<bridge profile>' : re.compile(r"root\['id'\]"),
    '<condenser posts>' : re.compile(r"root\[\d+\]\['post_id'\]"),
    '<condenser content>' : re.compile(r"root\['id'\]"), # condenser_api.get_content
    '<condenser replies>' : re.compile(r"root\[\d+\]\['id'\]"), # condenser_api.get_content_replies
    '<condenser blog>' : re.compile(r"root\[\d+\]\['comment'\]\['post_id'\]"), # condenser_api.get_blog
    '<condenser state>' : re.compile(r"root\['content'\]\[.+\]\['post_id'\]"), # condenser_api.get_state
    '<database posts>' : re.compile(r"root\['comments'\]\[\d+\]\['id'\]"),
    '<database votes>' : re.compile(r"root\['votes'\]\[\d+\]\['id'\]"),
    '<follow blog>' : re.compile(r"root\[\d+\]\['comment'\]\['post_id'\]"), # follow_api.get_blog
    '<tags posts>' : re.compile(r"root\[\d+\]\['post_id'\]"),
    '<tags post>' : re.compile(r"root\['post_id'\]") # tags_api.get_discussion
}

@pytest.mark.parametrize('api, method, request_body, pattern_json_path, output_path, negative, exclude_fields, filter_negative', gather_all_tests())
def test_pattern(config: Config, api: str, method: str, request_body : dict, pattern_json_path : Path, output_path: Path, negative: bool, exclude_fields: list, filter_negative : str):
    '''
    Some of fields here are duplicated, for example: api and method is already in request body, and negative is boolean representation of negative_filter
    Those arguments are because of filtering functionality in pytest, thanks to this it's possible to run tests like:

        pytest -k api/condenser_api
        pytest -k 'method/get_account_history and negative/False'
        pytest -k 'method/get_account_history or method/enum_virtual_ops and api/block_api'
    '''
    try:
        if not getenv('TAVERN_DISABLE_COMPARATOR', False):
            check_with_pattern(config, request_body, pattern_json_path, output_path, negative, exclude_fields)
        if getenv('TAVERN_GENERATE_PATTERNS', False):
            generate_pattern(config.node, request_body, negative, pattern_json_path)
    except Exception as e:
        tt.logger.error(f'While processing {pattern_json_path} got exception: {e}')
        raise e

def generate_pattern(config: Config, request : dict, negative : bool, output_path : Path):
    save_json(get_response(config, request, negative, True), output_path)

def check_with_pattern(config : Config, request_body : dict, pattern_json_path : Path, output_path: Path, negative: bool, exclude_fields: list):
    with pattern_json_path.open('rt') as file:
        pattern = json.load(file)

    try:
        response = get_response(config, request_body, negative, pattern is None)
    except Exception as e:
        tt.logger.error(f"Error occurred while gathering response: {e}")
        raise e

    try:
        response, _ = handle_excluded_fields(response, exclude_fields)
        pattern, exclude_fields = handle_excluded_fields(pattern, exclude_fields)
    except Exception as e:
        tt.logger.error(f'Error occurred while processing excluded fields: {e}')
        raise e

    save_json(response, output_path)
    assert are_objects_same(pattern, response, exclude_fields)
    remove(output_path)

def handle_excluded_fields(obj : dict, exclude_fields : Union[str, list]) -> Tuple[dict, Union[List[re.Pattern], Literal[None]]]:
    if isinstance(exclude_fields, str):
        exclude_fields = [PREDEFINED_IGNORE_TAGS.get(exclude_fields, None)]
    elif exclude_fields is not None:
        obj = remove_tag(obj, exclude_fields)
        exclude_fields = None
    return obj, exclude_fields
