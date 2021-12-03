def important_files_are_removed(node):
    paths_of_important_files = [
        '.',
        'config.ini',
        'stderr.txt',
    ]

    return all(not node.directory.joinpath(path).exists() for path in paths_of_important_files)


def unneeded_files_are_removed(node):
    paths_of_unneeded_files = [
        'blockchain/block_log',
    ]

    return all(not node.directory.joinpath(path).exists() for path in paths_of_unneeded_files)
