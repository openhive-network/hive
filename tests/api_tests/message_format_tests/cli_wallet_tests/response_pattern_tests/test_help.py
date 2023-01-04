from pathlib import Path

import test_tools as tt
from hive_local_tools.api.message_format.cli_wallet import verify_text_patterns

__PATTERNS_DIRECTORY = Path(__file__).with_name("help_response_patterns")


def test_help(wallet: tt.Wallet):
    help_content = wallet.api.help()
    verify_text_patterns(__PATTERNS_DIRECTORY, "help", help_content)
