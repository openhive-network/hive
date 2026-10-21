from __future__ import annotations

import pytest

from tests.generated_package import require_generated_package

require_generated_package()

from hiveio_api.operations import (  # noqa: E402
    AuthorRewardOperation,
    DeclinedVotingRightsOperation,
    FillConvertRequestOperation,
    ProducerMissedOperation,
    RecurrentTransferOperation,
    TransferOperation,
    VoteOperation,
)
from hiveio_api.operations.binary_filters import (  # noqa: E402
    BinaryFilterResult,
    any_operation_binary_filter,
    virtual_operation_binary_filter,
)


def test_numbers_of_operations_are_their_positions_in_operations_of_hived() -> None:
    # ACT & ASSERT - libraries/protocol/include/hive/protocol/operations.hpp
    assert VoteOperation.operation_id == 0
    assert TransferOperation.operation_id == 2
    assert RecurrentTransferOperation.operation_id == 49  # the last regular operation
    assert FillConvertRequestOperation.operation_id == 50  # the first virtual operation
    assert DeclinedVotingRightsOperation.operation_id == 92


def test_filter_of_any_operations_sets_bits_of_their_numbers() -> None:
    # ACT
    result = any_operation_binary_filter(VoteOperation, TransferOperation, ProducerMissedOperation)

    # ASSERT - producer_missed is the operation 86, bit 22 of the high half
    assert result == BinaryFilterResult(high=1 << 22, low=0b101)


def test_filter_of_virtual_operations_counts_bits_from_the_first_virtual_operation() -> None:
    # ACT
    result = virtual_operation_binary_filter(FillConvertRequestOperation, AuthorRewardOperation)

    # ASSERT - as `enum_vops_filter` of account_history_api of hived
    assert result == BinaryFilterResult(high=0, low=0b11)


def test_filters_of_no_operations_are_empty() -> None:
    # ACT & ASSERT
    assert any_operation_binary_filter() == BinaryFilterResult(high=0, low=0)
    assert virtual_operation_binary_filter() == BinaryFilterResult(high=0, low=0)


def test_filter_of_virtual_operations_rejects_regular_operations() -> None:
    # ACT & ASSERT
    with pytest.raises(ValueError, match="transfer"):
        virtual_operation_binary_filter(FillConvertRequestOperation, TransferOperation)  # type: ignore[arg-type]
