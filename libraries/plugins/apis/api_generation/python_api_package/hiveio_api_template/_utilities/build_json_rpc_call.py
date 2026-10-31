from __future__ import annotations


def build_json_rpc_call(*, method: str, params: str, id_: int = 0) -> str:
    """Builds params for jsonrpc call."""
    return (
        """{"id":"""
        + str(id_)
        + ""","jsonrpc":"2.0","method":\""""
        + method
        + '"'
        + (""","params":""" + params if params else "")
        + "}"
    )
