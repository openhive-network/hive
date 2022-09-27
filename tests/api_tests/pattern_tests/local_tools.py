import json
from dataclasses import dataclass
from os import getenv
from pathlib import Path
from pytest import param, mark
from typing import Iterable, List, Set, Tuple, Union
from urllib.parse import urljoin

import requests
import deepdiff
import test_tools as tt
import yaml

@dataclass
class Config:
    timeout: int
    node: tt.RemoteNode
    url : str

@dataclass
class TestDescription:
    api: str
    method: str
    request: dict
    pattern_json_path: Path
    output_path: Path
    is_negative: bool
    ignore_tags: Union[str, List[str]]

IS_DIRECT_CALL_HAFAH: bool = (getenv('IS_DIRECT_CALL_HAFAH', '').lower() == 'true')
CACHE_FOR_PROCESSED_FILES: List[TestDescription] = None

def get_marker(key: str, value):
    new_mark = getattr(mark, f'{key}/{value}')
    tt.logger.debug(f'preparing mark: {new_mark.name=} {new_mark.markname=}')
    return new_mark

def get_api_marker(api_name: str):
    return get_marker('api', api_name)

def get_method_marker(method_name: str):
    return get_marker('method', method_name)

def get_is_negative_marker(is_negative: bool):
    return get_marker('negative', is_negative)

def process_all_files() -> List[TestDescription]:
    global CACHE_FOR_PROCESSED_FILES
    if CACHE_FOR_PROCESSED_FILES is not None:
        return CACHE_FOR_PROCESSED_FILES
    else:
        CACHE_FOR_PROCESSED_FILES = []

    for file in get_path_to_tests().rglob('*.tavern.yaml'):
        tt.logger.info(f'Processing {file}')
        test_case = load_yaml(file)

        request = extract_from_path(test_case, 'stages', 0, 'request', 'json')
        api, method = str(request['method']).split('.')

        response = extract_from_path(test_case, 'stages', 0, 'response', 'verify_response_with')
        negative = response.get('extra_kwargs', {}).get('error_response', False)
        ignore_tags = response.get('extra_kwargs', {}).get('ignore_tags')

        pattern_json_path = file.parent / file.name.replace('.tavern.yaml', '.pat.json')
        output_path = file.parent / file.name.replace('.tavern.yaml', '.out.json')

        CACHE_FOR_PROCESSED_FILES.append(
            TestDescription(api, method, request, pattern_json_path, output_path, negative, ignore_tags)
        )
    return CACHE_FOR_PROCESSED_FILES


def gather_all_tests() -> List[Tuple]:
    tests = []
    for test in process_all_files():
        tests.append(
            param(
                test.request,
                test.pattern_json_path,
                test.output_path,
                test.is_negative,
                test.ignore_tags,
                marks=[
                    get_api_marker(test.api),
                    get_method_marker(test.method),
                    get_is_negative_marker(test.is_negative)
                ]
            )
        )
    return tests

def load_yaml(filename : Path) -> dict:
    with filename.open('rt') as file:
        return yaml.safe_load(file.read().replace('!', ''))

def extract_from_path(obj : Union[dict, list], *path):
    for item in path:
        assert not isinstance(item, int) or item < len(obj), f'object is shorter than {item}'
        assert not isinstance(item, str) or item in obj, f'object does not contain {item} key'
        obj = obj[item]
    return obj

def get_path_to_tests() -> Path:
    return Path(__file__).parent

def remove_tag(data, tags_to_remove : Iterable):
    tt.logger.debug(f'removing from: {data}')
    if not isinstance(data, (dict, list)):
        return data
    if isinstance(data, list):
        return [remove_tag(v, tags_to_remove) for v in data]
    return {k: remove_tag(v, tags_to_remove) for k, v in data.items() if k not in tags_to_remove}

def save_json(response : dict, output_path : Path):
    with output_path.open('wt') as file:
        json.dump(response, file, ensure_ascii=False, indent=2)

def hived_call(config: Config, api : str, method : str, params : Union[dict, list]):
    req_options = tt.RequestOptions(only_result=False,timeout=config.timeout,allow_error=True)
    endpoint = getattr(getattr(config.node.api, api.replace('_api', '')), method)
    return endpoint(**params, options=req_options) if isinstance(params, dict) else endpoint(*params, options=req_options)

def direct_call(config: Config, api : str, method : str, params : dict):
    return requests.post(urljoin(config.url, f'rpc/{method}'), json=params, timeout=config.timeout).json()

def call_api(config: Config, request : dict) -> dict:
    api, method = request['method'].split('.')
    params = request['params']
    retries = 5
    while retries:
        try:
            if IS_DIRECT_CALL_HAFAH:
                return direct_call(config, api, method, params)
            else:
                return hived_call(config, api, method, params)
        finally:
            retries -= 1
    assert retries != 0, 'Exceeded max amount of retries'

def get_response(config: Config, request : dict, negative : bool, allow_null : bool) -> dict:
    result = call_api(config, request)
    if not allow_null:
        assert (negative and 'error' in result) or (
            (not negative) and 'error' not in result and 'result' in result
        )
    result = result.get('result', result.get('error', None))
    if allow_null and result is None:
        tt.logger.warning(f'Detected empty response for {request}')
    return result

def are_objects_same(obj_1, obj_2, exclude_fields) -> bool:
    return 0 == len(deepdiff.DeepDiff(obj_1, obj_2, exclude_regex_paths=exclude_fields))
