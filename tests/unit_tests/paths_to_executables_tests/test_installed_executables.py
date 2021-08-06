def test_paths_of_installed_executables(paths, executables):
    for executable in executables:
        paths.set_installed_executables({executable.name: executable.path})
        assert paths.get_path_of(executable.name) == executable.path
