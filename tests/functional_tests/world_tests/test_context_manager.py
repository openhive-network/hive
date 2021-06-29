import pytest

from test_tools import automatic_tests_configuration, World


def test_explicit_close_call(request):
    directory = automatic_tests_configuration.get_preferred_directory(request)
    with World(directory=directory) as world:
        node = world.create_init_node()
        node.run()

        world.close()
        assert not node.is_running()


def test_entering_context_manager_scope_twice(world):
    with pytest.raises(RuntimeError):
        with world:
            pass


def test_closing_without_entering_context_manager_scope(request):
    directory = automatic_tests_configuration.get_preferred_directory(request)
    world = World(directory=directory)
    with pytest.raises(RuntimeError):
        world.close()
