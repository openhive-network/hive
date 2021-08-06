def test_paths_set_manually(paths, executables):
    for executable in executables:
        paths.set_path_of(executable.name, executable.path)
        assert paths.get_path_of(executable.name) == executable.path
