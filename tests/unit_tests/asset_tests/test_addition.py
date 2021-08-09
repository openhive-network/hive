import pytest
from test_tools import Asset


def test_addition_of_same_tokens():
    first = Asset.Hive(2)
    second = Asset.Hive(2)
    result = first + second

    assert first == second == Asset.Hive(2)  # Addends aren't modified
    assert result == Asset.Hive(4)
    assert isinstance(result, Asset.Hive)


def test_addition_of_different_tokens():
    with pytest.raises(RuntimeError):
        _ = Asset.Hive(2) + Asset.Test(2)


def test_addition_and_assignment_of_same_tokens():
    first = Asset.Hive(2)
    second = Asset.Hive(2)
    first += second

    assert first == Asset.Hive(4)
    assert second == Asset.Hive(2)  # Addend isn't modified


def test_addition_and_assignment_of_different_tokens():
    first = Asset.Hive(2)
    second = Asset.Test(2)

    with pytest.raises(RuntimeError):
        first += second
