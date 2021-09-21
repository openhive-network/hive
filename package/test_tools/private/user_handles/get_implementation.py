from test_tools.private.user_handles import RemoteNodeHandle


def get_implementation(handle):
    # pylint: disable=protected-access

    if isinstance(handle, RemoteNodeHandle):
        return handle._RemoteNodeHandle__implementation

    raise RuntimeError(f'Unable to get implementation for {handle}')
