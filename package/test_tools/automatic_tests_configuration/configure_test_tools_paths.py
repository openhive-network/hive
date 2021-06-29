from pathlib import Path

from test_tools import logger, World


def get_preferred_directory(request):
    if request.scope == 'function':
        current_directory = Path(request.module.__file__).parent
        module_name = Path(request.module.__file__).stem
        test_case_name = request.function.__name__

        return current_directory / ('generated_during_' + module_name) / test_case_name
    else:
        raise RuntimeError('Unsupported pytest.fixture scope, supported is only function scope')


def configure_test_tools_paths(request):
    default_directory = get_preferred_directory(request)

    logger.set_directory(default_directory)
    World.set_default_directory(default_directory)
