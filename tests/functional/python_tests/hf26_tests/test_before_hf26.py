import json
import pytest
import os
import subprocess

import test_tools as tt

from .local_tools import prepare_wallets, legacy_operation_passed, hf26_operation_failed

def test_before_hf26(prepare_network_before_hf26):
    wallet_legacy, wallet_hf26 = prepare_wallets(prepare_network_before_hf26)

    legacy_operation_passed(wallet_legacy)
    hf26_operation_failed(wallet_hf26)
