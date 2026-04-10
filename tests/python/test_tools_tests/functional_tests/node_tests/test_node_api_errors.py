from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

if TYPE_CHECKING:
    import test_tools as tt
from wax._private.api.overseer import WaxAssertionInResponseError


def test_if_raise_when_parameters_are_bad(node: tt.InitNode) -> None:
    with pytest.raises(WaxAssertionInResponseError):
        node.api.database.list_accounts(name=[])  # type: ignore[call-arg]
