from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

if TYPE_CHECKING:
    from beekeepy._handle import Beekeeper


def test_proper_closing(beekeeper: Beekeeper) -> None:
    # ARRANGE, ACT & ASSERT
    assert beekeeper is not None  # dummy assertion


@pytest.mark.parametrize("wallet_name", ["test", "123", "test"])  # noqa: PT014 we want to test duplicate wallet_name
def test_clean_dirs(beekeeper: Beekeeper, wallet_name: str) -> None:
    test_file = beekeeper.settings.working_directory / f"{wallet_name}.wallet"
    assert not test_file.exists()
    test_file.touch()
