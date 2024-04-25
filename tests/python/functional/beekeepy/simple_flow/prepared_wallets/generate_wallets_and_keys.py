from __future__ import annotations

import argparse
import asyncio
import json
import sys
from pathlib import Path

import test_tools as tt

from clive.__private.core.beekeeper import Beekeeper
from clive_local_tools.beekeeper.constants import MAX_BEEKEEPER_SESSION_AMOUNT
from clive_local_tools.data.generates import generate_wallet_name, generate_wallet_password
from clive_local_tools.data.models import Keys, WalletInfo


async def generate_wallets_and_keys(number_of_wallets: int, aliased_format: bool = True) -> None:
    """Function generate_wallets_and_keys is responsible of creating new wallets and keys needed for test_simple_flow in directories wallets/ and /keys . This function WILL NOT erase any content inside directories wallets/ and keys/."""
    wallets = [
        WalletInfo(name=generate_wallet_name(i), password=generate_wallet_password(i), keys=Keys(count=i % 5))
        for i in range(number_of_wallets)
    ]
    source_dir = Path(__file__).parent.resolve()
    async with await Beekeeper().launch(wallet_dir=source_dir / "wallets") as bk:
        for wallet in wallets:
            aliased = {}
            await bk.api.create(wallet_name=wallet.name, password=wallet.password)
            for key in wallet.keys.pairs:
                await bk.api.import_key(wallet_name=wallet.name, wif_key=key.private_key.value)
                if aliased_format:
                    aliased[f"{key.private_key.alias}"] = key.private_key.value
            if not aliased_format:
                extract_path = Path(f"{source_dir}/keys/{wallet.name}.org.keys")
                _ = await bk.export_keys_wallet(
                    wallet_name=wallet.name, wallet_password=wallet.password, extract_to=extract_path
                )
            else:
                # Save as alias format.
                extract_path = Path(f"{source_dir}/keys/{wallet.name}.alias.keys")
                with extract_path.open("w") as path:  # noqa: ASYNC101
                    json.dump(aliased, path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate wallets.")
    parser.add_argument(
        "--number-of-wallets",
        required=False,
        type=int,
        default=MAX_BEEKEEPER_SESSION_AMOUNT,
        help="Number of wallets to generate [default=MAX_BEEKEEPER_SESSION_AMOUNT(64)",
    )
    parser.add_argument(
        "--aliased",
        required=False,
        type=bool,
        default=False,
        help="Should keys should be dumped in aliased format alias=wif_key[default=False]",
    )

    args = parser.parse_args()

    if args.number_of_wallets <= 0:
        tt.logger.error("Value of number-of-wallets should be greater than 0. Aborting.")
        sys.exit(-1)
    asyncio.run(generate_wallets_and_keys(number_of_wallets=args.number_of_wallets, aliased_format=args.aliased))
