from __future__ import annotations

import filecmp
import json
import os
import shutil
from distutils.util import strtobool
from typing import TYPE_CHECKING, Callable, Final

import test_tools as tt
from hive_local_tools import PYTHON_TESTS_DIR

if TYPE_CHECKING:
    from pathlib import Path


PATTERNS_DIR: Final[Path] = (
    PYTHON_TESTS_DIR / "functional/transaction_serialization_tests/cli_wallet/transaction_pattern_tests"
)

VerifyPatternFunctionT = Callable[[tt.Wallet, str], None]


def __get_path_of_pattern_file(extension: str, type_of_serialization: str, pattern_name: str) -> str:
    return PATTERNS_DIR / f"dumped_files_{type_of_serialization}_wallet" / f"{pattern_name}.{extension}"


def __able_to_generate_pattern(validate_function: VerifyPatternFunctionT) -> VerifyPatternFunctionT:
    """
    This decorator, depending on `GENERATE_PATTERNS` environment variable, decides
    whether to call a wrapped function, which should validate a given transaction,
    or set newly generated file as new pattern
    """

    def impl(wallet: tt.Wallet, pattern_name: str) -> None:
        generate_patterns = strtobool(os.environ.get("GENERATE_PATTERNS", "OFF"))
        if not generate_patterns:
            return validate_function(wallet, pattern_name)

        for file_extension in ["json", "bin"]:
            shutil.move(
                wallet.directory / f"{pattern_name}.{file_extension}",
                __get_path_of_pattern_file(file_extension, wallet.transaction_serialization, pattern_name),
            )
        return None

    return impl


@__able_to_generate_pattern
def verify_generated_transaction_with_json_pattern(wallet: tt.Wallet, pattern_name: str) -> None:
    source_path_file = wallet.directory / f"{pattern_name}.json"
    target_path_file = __get_path_of_pattern_file("json", wallet.transaction_serialization, pattern_name)

    # Compare actual transaction with stored transaction
    with open(source_path_file) as file:
        actual_json = json.load(file)
    with open(target_path_file) as file:
        pattern_json = json.load(file)
    assert actual_json == pattern_json, f"{actual_json} does not match expected {pattern_json}"


@__able_to_generate_pattern
def verify_generated_transaction_with_binary_pattern(wallet: tt.Wallet, pattern_name: str) -> None:
    source_path_file = wallet.directory / f"{pattern_name}.bin"
    target_path_file = __get_path_of_pattern_file("bin", wallet.transaction_serialization, pattern_name)

    # Compare actual transaction with stored transaction
    assert filecmp.cmp(source_path_file, target_path_file, shallow=False)
