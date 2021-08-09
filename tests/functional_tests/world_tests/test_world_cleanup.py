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
        constants.WorldCleanUpPolicy.DO_NOT_REMOVE_FILES,
        remove_important_files=False,
        remove_unneeded_files=False
    )


def test_remove_only_unneeded_files_clean_up_policy(world):
    check_if_files_are_removed(
        world,
        constants.WorldCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES,
        remove_important_files=False,
        remove_unneeded_files=True
    )


def test_remove_everything_clean_up_policy(world):
    check_if_files_are_removed(
        world,
        constants.WorldCleanUpPolicy.REMOVE_EVERYTHING,
        remove_important_files=True,
        remove_unneeded_files=True
    )


def check_if_files_are_removed(world,
                               policy: constants.WorldCleanUpPolicy = None,
                               *,
                               remove_important_files: bool,
                               remove_unneeded_files: bool):
    # Create some nodes outside of a network
    init_node = world.create_init_node()
    api_node = world.create_api_node()

    # Create some nodes within a network
    network = world.create_network()
    network.create_init_node()
    network.create_api_node()

    init_node.run()
    api_node.run(wait_for_live=False)
    network.run()

    if policy is not None:
        world.set_clean_up_policy(policy)

    world.close()

    assert all(important_files_are_removed(node) == remove_important_files for node in network.nodes())
    assert all(unneeded_files_are_removed(node) == remove_unneeded_files for node in network.nodes())

    for network in world.networks():
        assert all(important_files_are_removed(node) == remove_important_files for node in network.nodes())
        assert all(unneeded_files_are_removed(node) == remove_unneeded_files for node in network.nodes())
