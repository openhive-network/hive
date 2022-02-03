from test_tools.private.user_handles.remote_node_handle import RemoteNodeHandle


def get_implementation(handle):
    # pylint: disable=protected-access

    if isinstance(handle, RemoteNodeHandle):
        return handle._RemoteNodeHandle__implementation

    raise RuntimeError(f'Unable to get implementation for {handle}')
