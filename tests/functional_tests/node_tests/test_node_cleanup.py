from test_tools import constants


def test_default_policy(world):
    init_node = world.create_init_node()
    init_node.run()

    world.close()

    assert not important_files_are_removed(init_node)
    assert unneeded_files_are_removed(init_node)


def test_do_not_remove_files_clean_up_policy(world):
    init_node = world.create_init_node()
    init_node.set_clean_up_policy(constants.NodeCleanUpPolicy.DO_NOT_REMOVE_FILES)
    init_node.run()

    world.close()

    assert not important_files_are_removed(init_node)
    assert not unneeded_files_are_removed(init_node)


def test_remove_only_unneeded_files_clean_up_policy(world):
    init_node = world.create_init_node()
    init_node.set_clean_up_policy(constants.NodeCleanUpPolicy.REMOVE_ONLY_UNNEEDED_FILES)
    init_node.run()

    world.close()

    assert not important_files_are_removed(init_node)
    assert unneeded_files_are_removed(init_node)


def test_remove_everything_clean_up_policy(world):
    init_node = world.create_init_node()
    init_node.set_clean_up_policy(constants.NodeCleanUpPolicy.REMOVE_EVERYTHING)
    init_node.run()

    world.close()

    assert important_files_are_removed(init_node)
    assert unneeded_files_are_removed(init_node)


def important_files_are_removed(node):
    paths_of_important_files = [
        '.',
        'config.ini',
        'stderr.txt',
    ]

    return all([not node.directory.joinpath(path).exists() for path in paths_of_important_files])


def unneeded_files_are_removed(node):
    paths_of_unneeded_files = [
        'blockchain/block_log',
        'p2p/peers.json',
    ]

    return all([not node.directory.joinpath(path).exists() for path in paths_of_unneeded_files])
