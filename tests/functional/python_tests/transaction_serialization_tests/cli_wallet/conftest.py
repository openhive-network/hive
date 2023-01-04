import logging

import pytest

import test_tools as tt


@pytest.fixture
def prepared_wallet(node, request):
    if request.param == "modern":
        yield tt.Wallet(attach_to=node, additional_arguments=["--transaction-serialization=hf26"])
    elif request.param == "legacy":
        yield tt.Wallet(attach_to=node, additional_arguments=["--transaction-serialization=legacy"])
    else:
        raise RuntimeError(f'Unsupported argument value "{request.param}", should be "modern" or "legacy"')


@pytest.fixture
def wallet_with_legacy_serialization(node, request):
    return tt.Wallet(
        attach_to=node,
        additional_arguments=[
            f"--store-transaction={request.fspath.purebasename}",
            "--transaction-serialization=legacy",
        ],
    )


@pytest.fixture
def wallet_with_hf26_serialization(node, request):
    return tt.Wallet(
        attach_to=node,
        additional_arguments=[f"--store-transaction={request.fspath.purebasename}", "--transaction-serialization=hf26"],
    )


@pytest.fixture(params=["legacy", "hf26"])
def wallet(node, request):
    type_of_serialization = request.param
    wallet = tt.Wallet(
        attach_to=node,
        additional_arguments=[
            f"--store-transaction={request.fspath.purebasename}",
            f"--transaction-serialization={type_of_serialization}",
        ],
    )

    return wallet
