from test_tools import constants
from test_tools.private.utilities_for_tests.node_files import important_files_are_removed, unneeded_files_are_removed


def test_default_policy(world):
    check_if_files_are_removed(
        world,
        remove_important_files=False,
        remove_unneeded_files=True
    )


def test_do_not_remove_files_clean_up_policy(world):
    check_if_files_are_removed(
        world,
        constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES,
        remove_important_files=False,
        remove_unneeded_files=False
    )


def test_remove_only_unneeded_files_clean_up_policy(world):
    check_if_files_are_removed(
        world,
        constants.NodeCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES,
        remove_important_files=False,
        remove_unneeded_files=True
    )


def test_remove_everything_clean_up_policy(world):
    check_if_files_are_removed(
        world,
        constants.NodeCleanUpPolicy.REMOVE_EVERYTHING,
        remove_important_files=True,
        remove_unneeded_files=True
    )


def check_if_files_are_removed(world,
                               policy: constants.NodeCleanUpPolicy = None,
                               *,
                               remove_important_files: bool,
                               remove_unneeded_files: bool):
    init_node = world.create_init_node()
    init_node.run()

    if policy is not None:
        init_node.set_clean_up_policy(policy)

    world.close()

    assert important_files_are_removed(init_node) == remove_important_files
    assert unneeded_files_are_removed(init_node) == remove_unneeded_files
