from __future__ import annotations

import pytest
from hiveio_api.validation.model import PreconfiguredBaseModel


class HumanizeTestCase(PreconfiguredBaseModel):
    field_a: str
    field_b: int | None
    field_c: int | None = None
    field_d: int = 1


@pytest.mark.parametrize(
    ("a", "b", "c", "expected_humanize_output"),
    (
        ("abc1", 2, 3, """field_a="abc1" field_b=2 field_c=3 field_d=1"""),
        ("abc4", None, 6, """field_a="abc4" field_c=6 field_d=1"""),
        ("abc7", 8, None, """field_a="abc7" field_b=8 field_d=1"""),
        ("abc0", 0, 0, """field_a="abc0" field_b=0 field_c=0 field_d=1"""),
        ("abc10", None, None, """field_a="abc10" field_d=1"""),
    ),
)
def test_humanize(a: str, b: int | None, c: int | None, expected_humanize_output: str) -> None:
    """
    This function returns a human-readable string representation of the Test model.
    """
    test_instance = HumanizeTestCase(field_a=a, field_b=b, field_c=c)
    assert test_instance.humanize() == expected_humanize_output
