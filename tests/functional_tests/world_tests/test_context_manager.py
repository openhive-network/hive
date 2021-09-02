import pytest

from test_tools import World
from test_tools.private.scope import context


def test_explicit_close_call():
    with World(directory=context.get_current_directory()) as world:
        node = world.create_init_node()
        node.run()

        world.close()
        assert not node.is_running()


def test_entering_context_manager_scope_twice(world):
    with pytest.raises(RuntimeError):
        with world:
            pass


def test_closing_without_entering_context_manager_scope():
    world = World(directory=context.get_current_directory())
    with pytest.raises(RuntimeError):
        world.close()
