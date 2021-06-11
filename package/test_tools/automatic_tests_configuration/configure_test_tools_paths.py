from pathlib import Path

from test_tools import logger, World

def configure_test_tools_paths(request):
    current_directory = Path(request.module.__file__).parent
    module_name = Path(request.module.__file__).stem
    test_case_name = request.function.__name__

    default_directory = current_directory / ('generated_during_' + module_name) / test_case_name

    logger.set_directory(default_directory)
    World.set_default_directory(default_directory)
