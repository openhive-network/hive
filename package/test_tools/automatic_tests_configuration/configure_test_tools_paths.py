from pathlib import Path
import pytest

from test_tools import logger, World


def get_preferred_directory(request):
    if request.scope == 'function':
        current_directory = Path(request.module.__file__).parent
        module_name = Path(request.module.__file__).stem
        test_case_name = request.function.__name__

        return current_directory / f'generated_during_{module_name}' / test_case_name
    elif request.scope == 'package':
        return __get_package_directory(request) / f'generated_by_fixture_{request.fixturename}'
    else:
        raise RuntimeError('Unsupported pytest.fixture scope, supported is only function and package scopes')


def __get_package_directory(request):
    assert request.scope == 'package'

    if isinstance(request.node, pytest.Session):
        package = request.node.items[0].parent.parent
    elif isinstance(request.node, pytest.Package):
        package = request.node
    else:
        raise RuntimeError(f'Internal error: request.node has unsupported type: {type(request.node)}')

    return Path(package.module.__file__).parent


def configure_test_tools_paths(request):
    default_directory = get_preferred_directory(request)

    logger.set_directory(default_directory)
