from __future__ import annotations

import argparse
import asyncio
import json
import sys
from pathlib import Path

from beekeepy import Settings
from beekeepy._handle import AsyncBeekeeper

import test_tools as tt
from hive_local_tools.beekeeper.models import WalletInfoWithImportedAccounts


async def generate_wallets_and_keys(number_of_wallets: int) -> None:
    """Function generate_wallets_and_keys is responsible of creating new wallets and keys needed for test_simple_flow in directories wallets/ and /keys . This function WILL NOT erase any content inside directories wallets/ and keys/."""
    wallets = [
        WalletInfoWithImportedAccounts(
            name=f"wallet-{i}",
            password=f"password-{i}",
            accounts=tt.Account.create_multiple(number_of_accounts=max(1, i % 5)),
        )
        for i in range(number_of_wallets)
    ]
    source_dir = Path(__file__).parent.resolve()
    with AsyncBeekeeper(settings=Settings(working_directory=source_dir / "wallets"), logger=tt.logger) as bk:
        for wallet in wallets:
            aliased = {}
            await bk.api.create(wallet_name=wallet.name, password=wallet.password)
            for account in wallet.accounts:
                await bk.api.import_key(wallet_name=wallet.name, wif_key=account.private_key)
                aliased[account.private_key] = account.public_key
            else:
                # Save as alias format.
                extract_path = Path(f"{source_dir}/keys/{wallet.name}.alias.keys")
                with extract_path.open("w") as path:
                    json.dump(aliased, path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate wallets.")
    parser.add_argument(
        "--number-of-wallets",
        required=False,
        type=int,
        default=5,
        help="Number of wallets to generate [default=MAX_BEEKEEPER_SESSION_AMOUNT(64)",
    )
    args = parser.parse_args()

    if args.number_of_wallets <= 0:
        tt.logger.error("Value of number-of-wallets should be greater than 0. Aborting.")
        sys.exit(-1)
    asyncio.run(generate_wallets_and_keys(number_of_wallets=args.number_of_wallets))
