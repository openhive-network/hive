from pathlib import Path
from typing import Optional

import pytest

from test_tools.private.scope import current_scope, ScopedCurrentDirectory


@pytest.fixture(autouse=True, scope='function')
def function_scope(request):
    with current_scope.create_new_scope(f'function: {__get_function_name(request)}'):
        ScopedCurrentDirectory(__get_directory_for_function(request))

        yield


@pytest.fixture(autouse=True, scope='module')
def module_scope(request):
    with current_scope.create_new_scope(f'module: {__get_module_name(request)}'):
        ScopedCurrentDirectory(__get_directory_for_module(request))

        yield


@pytest.fixture(autouse=True, scope='package')
def package_scope(request):
    if not __is_run_in_package(request):
        # Fixtures with package scope are run also for tests which are not in any
        # package. If this is the case, package scope shouldn't be created.
        yield
    else:
        with current_scope.create_new_scope(f'package: {__get_package_name(request)}'):
            ScopedCurrentDirectory(__get_directory_for_package(request))

            yield


def __is_run_in_package(request) -> bool:
    return __get_pytest_package_object(request) is not None


def __get_package_name(request) -> str:
    return __get_pytest_package_object(request).name


def __get_directory_for_package(request) -> Path:
    return Path(__get_pytest_package_object(request).fspath).parent / 'generated_by_package_fixtures'


def __get_directory_for_module(request):
    assert request.scope == 'module'
    module_name = Path(request.module.__file__).stem
    module_directory = Path(request.module.__file__).parent
    return module_directory / f'generated_during_{module_name}'


def __get_module_name(request) -> str:
    assert request.scope == 'module'
    return Path(request.module.__file__).stem


def __get_directory_for_function(request):
    assert request.scope == 'function'
    function_name = request.function.__name__
    return current_scope.context.get_current_directory() / function_name


def __get_function_name(request) -> str:
    assert request.scope == 'function'
    return request.node.name


def __get_pytest_package_object(request) -> Optional[pytest.Package]:
    assert request.scope == 'package'

    pytest_scope = request.node.items[0]
    while pytest_scope is not None:
        if isinstance(pytest_scope, pytest.Package):
            return pytest_scope

        pytest_scope = pytest_scope.parent

    return None
